#include "Profiling.h"

void Profiler::BeginSession(const std::string& name)
{
    if (m_ActiveSession)
        EndSession();

    m_SessionName = name;
    if (!std::filesystem::exists(m_OutputDirectoryPath))
        std::filesystem::create_directories(m_OutputDirectoryPath);
    
    m_OutputStream.open(m_OutputDirectoryPath.string() + name + ".json");
    WriteHeader();

    m_ActiveSession = true;
}

void Profiler::EndSession()
{
    if (!m_ActiveSession) { return; }
    WriteFooter();
    m_OutputStream.close();
    m_SessionName = "None";
    m_OutputStream.close();
    m_ProfileCount = 0;
}

void Profiler::WriteProfile(const ProfileResult & result)
{
    std::lock_guard<std::mutex> lock(m_Lock);

    if (m_ProfileCount++ > 0)
        m_OutputStream << ",";

    std::string name = result.Name;
    uint64_t threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::replace(name.begin(), name.end(), '"', '\'');

    m_OutputStream << "{";
    m_OutputStream << "\"cat\":\"function\",";
    m_OutputStream << "\"dur\":" << (result.End - result.Start) << ',';
    m_OutputStream << "\"name\":\"" << name << "\",";
    m_OutputStream << "\"ph\":\"X\",";
    m_OutputStream << "\"pid\":0,";
    m_OutputStream << "\"tid\":" << threadID << ",";
    m_OutputStream << "\"ts\":" << result.Start;
    m_OutputStream << "}";

    m_OutputStream.flush();
}

void Profiler::WriteHeader()
{
    m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
    m_OutputStream.flush();
}

void Profiler::WriteFooter()
{
    m_OutputStream << "]}";
    m_OutputStream.flush();
}

void InstrumentationTimer::Stop()
{
    if (m_Stopped)
        return;

    auto endTimepoint = std::chrono::high_resolution_clock::now();

    long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
    long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

    auto threadID = std::hash<std::thread::id>{}(std::this_thread::get_id());
    Profiler::Get().WriteProfile({ m_Name, start, end, std::this_thread::get_id() });
    m_Stopped = true;
}
