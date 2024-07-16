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

    void Run();

    void PushLayer(class Layer* layer);
    void PushOverlay(class Layer* overlay);
    void PopLayer(class Layer* layer);
    void PopOverlay(class Layer* overlay);

protected:
    [[nodiscard]] bool OnWindowClose(WindowCloseEvent& e);

private:
    ApplicationSettings m_Settings;
    std::unique_ptr<Window> m_Window;
    LayerStack m_LayerStack;
    std::unique_ptr<class EventHub> m_EventHub;
    Clock m_Clock;

private:
    static ApplicationBase* s_Instance;
};

ApplicationBase* CreateApplication();
}
