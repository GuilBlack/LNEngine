#include "ApplicationBase.h"

#include "Core/Window.h"

#include "Utils/Profiling.h"
#include "Utils/Log.h"
#include "Utils/_Defines.h"
#include "LayerStack.h"
#include "Layer.h"

namespace lne
{
ApplicationBase* ApplicationBase::s_Instance = nullptr;

ApplicationBase::ApplicationBase(ApplicationSettings&& settings)
    : m_Settings(std::move(settings))
{
    LNE_ASSERT(!s_Instance, "Application already exists");
    s_Instance = this;

    Profiler::Get().BeginSession("Init");
    LNE_PROFILE_FUNCTION();

    Log::Init();
    m_EventHub.reset(new EventHub());

    m_EventHub->RegisterListener<WindowCloseEvent>(this, &ApplicationBase::OnWindowClose);

    glfwSetErrorCallback([](int errCode, const char* description)
        {
            LNE_ERROR("GLFW Error ({0}): {1}", errCode, std::string_view{ description });
        });

    if (!glfwInit())
        LNE_ASSERT(false, "Failed to initialize GLFW");

    LNE_INFO("glfw initialized");

    m_Window.reset(new Window({ m_Settings.Name, m_Settings.Width, m_Settings.Height }));

    LNE_INFO("Application {0} initialized", m_Settings.Name);
    Profiler::Get().EndSession();
}

ApplicationBase::~ApplicationBase()
{
    m_EventHub->UnregisterListener<WindowCloseEvent>(this);
    m_LayerStack.Clear();
    m_Window.reset();
    glfwTerminate();

    LNE_INFO("Application {0} nuked", m_Settings.Name);
    Log::Nuke();
}

void ApplicationBase::Run()
{
    Profiler::Get().BeginSession("Run");
    LNE_PROFILE_FUNCTION();

    while (!m_Window->ShouldClose())
    {
        LNE_PROFILE_SCOPE("Run Loop");
        for (auto layer : m_LayerStack)
        {
            layer->OnUpdate();
        }

        m_Window->PollEvents();
    }

    LNE_PROFILE_FUNCTION_END();
    Profiler::Get().EndSession();
}

void ApplicationBase::PushLayer(Layer* layer)
{
    m_LayerStack.PushLayer(layer);
}

void ApplicationBase::PushOverlay(Layer* overlay)
{
    m_LayerStack.PushOverlay(overlay);
}

void ApplicationBase::PopLayer(Layer* layer)
{
    m_LayerStack.PopLayer(layer);
}

void ApplicationBase::PopOverlay(Layer* overlay)
{
    m_LayerStack.PopOverlay(overlay);
}

bool ApplicationBase::OnWindowClose(WindowCloseEvent& e)
{
    LNE_INFO("WindowCloseEvent received");
    return false;
}

}
