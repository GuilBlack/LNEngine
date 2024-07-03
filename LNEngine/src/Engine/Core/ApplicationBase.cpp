#include "ApplicationBase.h"
#include "Utils/Profiling.h"
#include "Utils/Log.h"

namespace lne
{


void ApplicationBase::Run()
{
    Profiler::Get().BeginSession("Run");
    LNE_PROFILE_FUNCTION();

    for (uint16_t i = 0; i < 100; i++)
    {
    }

    LNE_PROFILE_FUNCTION_END();
    Profiler::Get().EndSession();
}
void ApplicationBase::Init()
{
    Profiler::Get().BeginSession("Init");
    LNE_PROFILE_FUNCTION();

    Log::Init();

    LNE_INFO("Application {0} initialized", m_Settings.Name);
    Profiler::Get().EndSession();
}
void ApplicationBase::Shutdown()
{}
}
