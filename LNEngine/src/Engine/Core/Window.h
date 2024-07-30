#pragma once

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

    [[nodiscard]] std::shared_ptr<class GfxContext> GetGfxContext() const { return m_GfxContext; }
    [[nodiscard]] std::shared_ptr<class Swapchain> GetSwapchain() const { return m_SwapChain; }

    [[nodiscard]] uint32_t GetWidth() const { return m_Settings.Width; }
    [[nodiscard]] uint32_t GetHeight() const { return m_Settings.Height; }

    void PollEvents() const;
    void BeginFrame() const;
    void Present();
    [[nodiscard]] bool ShouldClose() const;

private:
    struct GLFWwindow*  m_Handle;
    WindowSettings      m_Settings;
    bool                m_IsDirty{ false };

    std::unique_ptr<class InputManager> m_InputManager;
    std::shared_ptr<class Swapchain>    m_SwapChain;
    std::shared_ptr<class GfxContext>   m_GfxContext;

private:
    void InitEventCallbacks();
    bool OnWindowResize(class WindowResizeEvent& e);

    friend class ApplicationBase;
};
};
