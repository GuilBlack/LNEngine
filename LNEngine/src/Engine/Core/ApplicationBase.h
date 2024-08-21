#pragma once
#include "LayerStack.h"
#include "Window.h"
#include "Events/EventHub.h"
#include "Events/WindowEvents.h"
#include "Inputs/Inputs.h"
#include "Clock.h"

namespace lne
{
struct ApplicationSettings
{
    std::string Name;
    uint32_t    Width{};
    uint32_t    Height{};
    bool        IsResizable{};
};

class ApplicationBase
{
public:
    explicit ApplicationBase(ApplicationSettings&& settings);
    virtual ~ApplicationBase();

    [[nodiscard]] static ApplicationBase& Get() { return *s_Instance; }
    [[nodiscard]] static EventHub& GetEventHub() { return *s_Instance->m_EventHub; }
    [[nodiscard]] static class InputManager& GetInputManager() { return *s_Instance->m_Window->m_InputManager; }
    [[nodiscard]] static class Clock& GetClock() { return s_Instance->m_Clock; }
    [[nodiscard]] static class Window& GetWindow() { return *s_Instance->m_Window; }
    [[nodiscard]] static class Renderer& GetRenderer() { return *s_Instance->m_Renderer; }
    [[nodiscard]] static const std::string& GetAssetsPath();

    void Run();

    /// <summary>
    /// Pushes a layer to the top of the layer stack.
    /// the default priority for event callbacks given to layers is 0.
    /// </summary>
    /// <param name="layer"></param>
    void PushLayer(class Layer* layer);

    /// <summary>
    /// Pushes an overlay to the top of the layer stack.
    /// the default priority for event callbacks given to overlays is -10 to ensure they are called before the normal layers.
    /// </summary>
    /// <param name="overlay"></param>
    void PushOverlay(class Layer* overlay);
    void PopLayer(class Layer* layer);
    void PopOverlay(class Layer* overlay);

protected:
    [[nodiscard]] bool OnWindowClose(WindowCloseEvent& e);

private:
    ApplicationSettings m_Settings;
    Clock m_Clock;
    LayerStack m_LayerStack;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<class EventHub> m_EventHub;
    std::unique_ptr<class Renderer> m_Renderer;
    std::unique_ptr<class ImGuiService> m_ImGuiService;
private:
    static ApplicationBase* s_Instance;
};

ApplicationBase* CreateApplication();
}
