#include "ApplicationBase.h"

#include "Core/Window.h"

#include "Utils/Profiling.h"
#include "Utils/Log.h"
#include "Utils/Defines.h"

namespace lne
{
void ApplicationBase::Run()
{
    Profiler::Get().BeginSession("Run");
    LNE_PROFILE_FUNCTION();

    while (!m_Window->ShouldClose())
    {
        LNE_PROFILE_SCOPE("Run Loop");
        m_Window->PollEvents();
    }

    LNE_PROFILE_FUNCTION_END();
    Profiler::Get().EndSession();
}

void ApplicationBase::Init()
{
    Profiler::Get().BeginSession("Init");
    LNE_PROFILE_FUNCTION();

    Log::Init();

    glfwSetErrorCallback([](int errCode, const char* description)
    {
        LNE_ERROR("GLFW Error ({0}): {1}", errCode, std::string_view{description});
    });
    
    if (!glfwInit())
        LNE_ASSERT(false, "Failed to initialize GLFW");

    LNE_INFO("glfw initialized");

    m_Window = new Window({m_Settings.Name, m_Settings.Width, m_Settings.Height});

    LNE_INFO("Application {0} initialized", m_Settings.Name);
    Profiler::Get().EndSession();
}

void ApplicationBase::Nuke()
{
    delete m_Window;
    glfwTerminate();

    LNE_INFO("Application {0} nuked", m_Settings.Name);
    Log::Nuke();
}
}
