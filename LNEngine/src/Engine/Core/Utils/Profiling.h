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

#if defined(TRACY_ENABLE) && defined(LNE_DEBUG)
#   define LNE_PROFILE_SCOPE(name) ZoneScopedN(name);
#   define LNE_PROFILE_SCOPE_C(name, color) ZoneScopedNC(name, color);
#   define LNE_PROFILE_FUNCTION() LNE_PROFILE_SCOPE(__FUNCSIG__);
#   define LNE_PROFILE_FUNCTION_C(color) LNE_PROFILE_SCOPE_C(__FUNCSIG__, color);
#   define LNE_PROFILE_SCOPE_STR(name) ZoneScoped; ZoneName(name.c_str(), name.size());
#   define LNE_PROFILE_SCOPE_STR_C(name, color) ZoneScoped; ZoneName(name.c_str(), name.size()); ZoneColor(color);
#   define LNE_PROFILE_FRAME FrameMark;
#else
#   define LNE_PROFILE_SCOPE(name)
#   define LNE_PROFILE_FUNCTION()
#   define LNE_PROFILE_FRAME
#endif
