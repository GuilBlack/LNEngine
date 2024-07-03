#pragma once

struct ProfileResult
{
    const std::string   Name;
    long long           Start, End;
    std::thread::id     ThreadID;
};

struct ProfilingSession
{
    std::string Name;
};

class Profiler
{
public:
    ~Profiler()
    {
        EndSession();
    }

    static Profiler& Get()
    {
        static Profiler instance;
        return instance;
    }

    void BeginSession(const std::string& name);
    void EndSession();
    void WriteProfile(const ProfileResult& result);

    void WriteHeader();

    void WriteFooter();

    void SetOutputDirectoryPath(const std::filesystem::path path)
    {
        m_OutputDirectoryPath = path;
    }

private:
    std::string             m_SessionName{ "None" };
    std::filesystem::path   m_OutputDirectoryPath;
    std::ofstream           m_OutputStream;
    int32_t                 m_ProfileCount{ 0 };
    std::mutex              m_Lock;
    bool                    m_ActiveSession{ false };

private:
    Profiler()
    {
        m_OutputDirectoryPath = std::filesystem::current_path().string() + "/Profiling/";
    }
};

class InstrumentationTimer
{
public:
    InstrumentationTimer(std::string name)
        : m_Name(name)
    {
        m_StartTimepoint = std::chrono::high_resolution_clock::now();
    }

    ~InstrumentationTimer()
    {
        if (!m_Stopped)
            Stop();
    }

    void Stop();

private:
    std::string m_Name;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
    bool m_Stopped{ false };
};

#define LNE_PROFILE_SCOPE(name) InstrumentationTimer timer##__LINE__(name)
#define LNE_PROFILE_FUNCTION() LNE_PROFILE_SCOPE(__FUNCSIG__)

#define LNE_PROFILE_SCOPE_END() timer##__LINE__.Stop()
#define LNE_PROFILE_FUNCTION_END() LNE_PROFILE_SCOPE_END()
