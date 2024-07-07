#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
    void OnAttach() override
    {
        APP_INFO("AppLayer::OnAttach");
        m_InputManager.reset(new lne::InputManager());
    }

    void OnDetach() override
    {
        APP_INFO("AppLayer::OnDetach");
    }

    void OnUpdate() override
    {
        float x, y;
        m_InputManager->GetMousePosition(x, y);
        APP_TRACE("Mouse Position: {0}, {1}", x, y);
    }

private:
    std::unique_ptr<lne::InputManager> m_InputManager;
};

class Application : public lne::ApplicationBase
{
public:
    Application(lne::ApplicationSettings&& settings)
        : lne::ApplicationBase(std::move(settings))
    {
        PushLayer(new AppLayer());
    }
};

lne::ApplicationBase* lne::CreateApplication()
{
    return new Application({
        "LNApp",
        1280, 720
    });
}
