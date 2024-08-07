#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
public:
    AppLayer()
        : Layer("AppLayer")
    {}
    void OnAttach() override
    {
        APP_INFO("AppLayer::OnAttach");
        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();
        lne::GraphicsPipelineDesc desc{};
        desc.Name = "HelloTriangle";
        desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders/";
        desc.AddStage(lne::EShaderStage::Vertex)
            .AddStage(lne::EShaderStage::Fragment);
        desc.SetCulling(lne::ECullMode::Back)
            .SetWinding(lne::EWindingOrder::CounterClockwise)
            .SetFill(lne::EFillMode::Solid);
        desc.Framebuffer = fb;
        desc.Blend.EnableBlend(false);
        fb.SetClearColor({ 1.0f, 0.0f, 1.0f, 1.0f });
        m_Pipeline = lne::ApplicationBase::GetRenderer().CreateGraphicsPipeline(desc);
    }

    void OnDetach() override
    {
        APP_INFO("AppLayer::OnDetach");
        m_Pipeline.Reset();
    }

    void OnUpdate(float deltaTime) override
    {
        static int frameIndex = 0;
        ++frameIndex;
        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);

        lne::ApplicationBase::GetRenderer().Draw(m_Pipeline);

        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

private:
    lne::SafePtr<lne::GraphicsPipeline> m_Pipeline;
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
