#pragma once
#include "Engine/Graphics/Framebuffer.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{
struct WindowSettings
{
    std::string Name;
    uint32_t    Width{}, Height{};
    bool        Resizable{ false };
    bool        Fullscreen{ false };
};

class Window final
{
public:
    Window(WindowSettings&& settings);
    ~Window();

    [[nodiscard]] SafePtr<class GfxContext> GetGfxContext() const { return m_GfxContext; }
    [[nodiscard]] SafePtr<class Swapchain> GetSwapchain() const { return m_SwapChain; }
    [[nodiscard]] struct GLFWwindow* GetHandle() const { return m_Handle; }

    [[nodiscard]] uint32_t GetWidth() const { return m_Settings.Width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_Settings.Height; }

    [[nodiscard]] Framebuffer& GetCurrentFramebuffer() const;

    void PollEvents() const;
    void BeginFrame() const;
    void Present();
    [[nodiscard]] bool ShouldClose() const;

    void AddSwapchainRecreateCallback(void* key, std::function<void()> callback);
    void RemoveSwapchainRecreateCallback(void* key);

private:
    struct GLFWwindow*  m_Handle;
    WindowSettings      m_Settings;
    bool                m_IsDirty{ false };

    std::unique_ptr<class InputManager> m_InputManager;
    SafePtr<class Swapchain>    m_SwapChain;
    SafePtr<class GfxContext>   m_GfxContext;

    std::map<void*, std::function<void()>> m_SwapchainRecreateCallback;

private:
    void InitEventCallbacks();
    bool OnWindowResize(class WindowResizeEvent& e);

    friend class ApplicationBase;
};
};
