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

    void OnUpdate(float deltaTime) override
    {
        static int frameIndex = 0;
        ++frameIndex;
        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        vk::ClearColorValue clearValue;
        float flash = std::abs(std::sin(frameIndex / 120.f));
        clearValue = { 0.0f, 0.0f, flash, 1.0f };
        fb.SetClearColor(clearValue);
        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);
        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

private:
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
