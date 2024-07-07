#pragma once
#include "LayerStack.h"
#include "Window.h"
#include "Events/EventHub.h"
#include "Events/WindowEvents.h"

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

    static ApplicationBase& Get() { return *s_Instance; }
    static EventHub& GetEventHub() { return *s_Instance->m_EventHub; }

    void Run();

    void PushLayer(class Layer* layer);
    void PushOverlay(class Layer* overlay);
    void PopLayer(class Layer* layer);
    void PopOverlay(class Layer* overlay);

protected:
    bool OnWindowClose(WindowCloseEvent& e);

private:
    ApplicationSettings m_Settings;
    std::unique_ptr<Window> m_Window;
    LayerStack m_LayerStack;
    std::unique_ptr<class EventHub> m_EventHub;

private:
    static ApplicationBase* s_Instance;
};

ApplicationBase* CreateApplication();
}
