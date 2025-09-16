#include "ApplicationBase.h"

#include "Core/Window.h"

#include "Utils/Profiling.h"
#include "Utils/Log.h"
#include "Utils/_Defines.h"
#include "LayerStack.h"
#include "Layer.h"
#include "Core/Events/ApplicationEvents.h"
#include "Graphics/Renderer.h"
#include "Graphics/CommandPoolManager.h"
#include "Graphics/ImGui/ImGuiService.h"
#include "Graphics/GfxContext.h"

namespace lne
{
ApplicationBase* ApplicationBase::s_Instance = nullptr;
std::string s_AssetsPath;

void ApplicationBase::PinnedTaskRunner::Execute()
{
    while (TaskScheduler.lock()->GetIsShutdownRequested() == false && IsFinished == false)
    {
        if (auto ts = TaskScheduler.lock())
        {
            ts->WaitForNewPinnedTasks();
            ts->RunPinnedTasks();
        }
    }
}

ApplicationBase::ApplicationBase(ApplicationSettings&& settings)
    : m_Settings(std::move(settings))
{
    LNE_ASSERT(!s_Instance, "Application already exists");
    s_Instance = this;
    s_AssetsPath = std::filesystem::current_path().string() + "\\Assets\\";

    LNE_PROFILE_FUNCTION();

    Log::Init();
    m_EventHub.reset(lnnew EventHub());

    m_EventHub->RegisterListener<WindowCloseEvent>(this, &ApplicationBase::OnWindowClose);

    m_TaskScheduler.reset(lnnew enki::TaskScheduler());
    m_TaskScheduler->Initialize();

    m_PinnedTaskRunner.reset(lnnew PinnedTaskRunner(m_TaskScheduler));
    m_TaskScheduler->AddPinnedTask(m_PinnedTaskRunner.get());

    glfwSetErrorCallback([](int errCode, const char* description)
        {
            LNE_ERROR("GLFW Error ({0}): {1}", errCode, std::string_view{ description });
        });

    LNE_ASSERT(glfwInit(), "Failed to initialize GLFW");
    LNE_INFO("glfw initialized");

    LNE_ASSERT(GfxContext::InitVulkan(m_Settings.Name), "Failed to initialize Vulkan");
    LNE_INFO("Vulkan initialized");

    m_Window.reset(lnnew Window({ m_Settings.Name, m_Settings.Width, m_Settings.Height, m_Settings.IsResizable }));

    m_Renderer.reset(lnnew Renderer());
    m_Renderer->Init(m_Window, m_TaskScheduler);

    m_ImGuiService.reset(lnnew ImGuiService());
    m_ImGuiService->Init(m_Window);

    LNE_INFO("Application {0} initialized", m_Settings.Name);
}

ApplicationBase::~ApplicationBase()
{
    m_EventHub->UnregisterListener<WindowCloseEvent>(this);
    m_LayerStack.Clear();
    m_Renderer.reset();
    m_Window.reset();
    glfwTerminate();
    LNE_INFO("Application {0} nuked", m_Settings.Name);
    Log::Nuke();
}

class InputManager& ApplicationBase::GetInputManager()
{
    return *s_Instance->m_Window->m_InputManager;
}

class Window& ApplicationBase::GetWindow()
{
    return *s_Instance->m_Window;
}

class Renderer& ApplicationBase::GetRenderer()
{
    return *s_Instance->m_Renderer;
}

const std::string& ApplicationBase::GetAssetsPath()
{
    return s_AssetsPath;
}

void ApplicationBase::Run()
{
    auto graphicsContext = m_Renderer->GetGfxContext();
    CommandPoolManager& cpManager = graphicsContext->GetCommandPoolManager();
    cpManager.ResetFrameCommands(0);
    vk::CommandBuffer cb = cpManager.BeginOrGetPrimaryFrameCommandBuffer(0);
    graphicsContext->UploadDefaultResources();
    m_Renderer->InitResources();
    m_ImGuiService->CreateFontsTexture();

    for (auto layer : m_LayerStack)
        layer->OnAttach();

    FrameCommands fc = cpManager.EndFrame(0);
    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBuffers(fc.CommandBuffers);
    graphicsContext->SubmitToQueue(EQueueFamilyType::Graphics, submitInfo, fc.Fence);
    graphicsContext->GetDevice().waitForFences(fc.Fence, VK_TRUE, UINT64_MAX);

    m_Clock.Start();

    while (!m_Window->ShouldClose())
    {
        LNE_PROFILE_SCOPE("Frame")
        m_Clock.Tick();

        m_Window->BeginFrame();
        m_Renderer->BeginFrame();
        
        for (auto layer : m_LayerStack)
            layer->OnUpdate(m_Clock.GetDeltaTime());

        {
            LNE_PROFILE_SCOPE("ImGui")
            m_ImGuiService->BeginFrame();
                for (auto layer : m_LayerStack)
                    layer->OnImGuiRender();
            m_ImGuiService->EndFrame();
        }

        m_Renderer->EndFrame();
        m_Window->Present();
		m_Renderer->WaitForRenderTasksToFinish();
		m_Renderer->RunRenderTasks();
        m_Renderer->PostFrame();

        auto appUpdatedEvent = AppUpdatedEvent();
        m_EventHub->FireEvent(appUpdatedEvent);
        m_Window->PollEvents();
        LNE_PROFILE_FRAME
    }

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
    m_PinnedTaskRunner->IsFinished = true;
    m_TaskScheduler->WaitforAllAndShutdown();
    auto gfxContext = m_Window->GetGfxContext();
    gfxContext->WaitIdle();

    m_Window->GetGfxContext()->NukeDefaultResources();
    m_Renderer->NukeResources();

    for (auto layer : m_LayerStack)
        layer->OnDetach();

    m_ImGuiService->Nuke();
    m_Renderer->Nuke();
    return false;
}

}
