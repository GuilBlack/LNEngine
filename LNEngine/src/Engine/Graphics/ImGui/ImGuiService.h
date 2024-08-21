#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Events/Events.h"
#include "Engine/Core/Events/WindowEvents.h"

namespace lne
{
class ImGuiService
{
public:
    ImGuiService();
    ~ImGuiService();

    void Init(std::unique_ptr<class Window>& window);
    void Nuke();

    void BeginFrame();
    void EndFrame();

    bool OnWindowResized(WindowResizeEvent& windowResize);

private:
    SafePtr<class GfxContext> m_GraphicsContext;
    SafePtr<class Swapchain> m_Swapchain;
    vk::DescriptorPool m_DescriptorPool;
    std::vector<class Framebuffer> m_Framebuffers;
};
}

