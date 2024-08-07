#include "ApplicationBase.h"

#include "Core/Window.h"

#include "Utils/Profiling.h"
#include "Utils/Log.h"
#include "Utils/_Defines.h"
#include "LayerStack.h"
#include "Layer.h"
#include "Core/Events/ApplicationEvents.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Renderer.h"
#include "Graphics/CommandBufferManager.h"
#include "Graphics/Texture.h"

namespace lne
{
ApplicationBase* ApplicationBase::s_Instance = nullptr;
std::string s_AssetsPath;

ApplicationBase::ApplicationBase(ApplicationSettings&& settings)
    : m_Settings(std::move(settings))
{
    LNE_ASSERT(!s_Instance, "Application already exists");
    s_Instance = this;
    s_AssetsPath = std::filesystem::current_path().string() + "/Assets/";

    Profiler::Get().BeginSession("Init");
    LNE_PROFILE_FUNCTION();

    Log::Init();
    m_EventHub.reset(lnnew EventHub());

    m_EventHub->RegisterListener<WindowCloseEvent>(this, &ApplicationBase::OnWindowClose);

    glfwSetErrorCallback([](int errCode, const char* description)
        {
            LNE_ERROR("GLFW Error ({0}): {1}", errCode, std::string_view{ description });
        });

    LNE_ASSERT(glfwInit(), "Failed to initialize GLFW");
    LNE_INFO("glfw initialized");

    LNE_ASSERT(GfxContext::InitVulkan(settings.Name), "Failed to initialize Vulkan");
    LNE_INFO("Vulkan initialized");

    m_Window.reset(lnnew Window({ m_Settings.Name, m_Settings.Width, m_Settings.Height }));

    m_Renderer.reset(lnnew Renderer());
    m_Renderer->Init(m_Window);

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

const std::string& ApplicationBase::GetAssetsPath()
{
    return s_AssetsPath;
}

void ApplicationBase::Run()
{
    Profiler::Get().BeginSession("Run");
    LNE_PROFILE_FUNCTION();
    m_Clock.Start();

    while (!m_Window->ShouldClose())
    {
        LNE_PROFILE_SCOPE("Run Loop");
        m_Clock.Tick();

        m_Window->BeginFrame();
        m_Renderer->BeginFrame();
        
        for (auto layer : m_LayerStack)
            layer->OnUpdate(m_Clock.GetDeltaTime());

        m_Renderer->EndFrame();
        m_Window->Present();

        auto appUpdatedEvent = AppUpdatedEvent();
        m_EventHub->FireEvent(appUpdatedEvent);
        m_Window->PollEvents();
    }

    LNE_PROFILE_FUNCTION_END();
    Profiler::Get().EndSession();
}

void ApplicationBase::PushLayer(Layer* layer)
{
    m_LayerStack.PushLayer(layer);
    layer->BindEventCallbacks(0);
}

void ApplicationBase::PushOverlay(Layer* overlay)
{
    m_LayerStack.PushOverlay(overlay);
    overlay->BindEventCallbacks(-10);
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
    m_Renderer->Shutdown();
    return false;
}

}
