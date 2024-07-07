#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
    void OnAttach() override
    {
        APP_INFO("AppLayer::OnAttach");
    }

    void OnDetach() override
    {
        APP_INFO("AppLayer::OnDetach");
    }

    void OnUpdate() override
    {
    }
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
