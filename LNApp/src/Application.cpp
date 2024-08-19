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
        desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders\\HelloTriangle.glsl";
        desc.AddStage(lne::ShaderStage::eVertex)
            .AddStage(lne::ShaderStage::eFragment);
        desc.SetCulling(lne::ECullMode::Back)
            .SetWinding(lne::EWindingOrder::CounterClockwise)
            .SetFill(lne::EFillMode::Solid);
        desc.Framebuffer = fb;
        desc.Blend.EnableBlend(false);
        fb.SetClearColor({ 1.0f, 0.0f, 1.0f, 1.0f });
        m_Pipeline = lne::ApplicationBase::GetRenderer().CreateGraphicsPipeline(desc);

        struct Vertex {
            glm::vec4 Position;
            glm::vec4 Color;
        };

        std::vector<Vertex> vertices = {
            { { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.5f, 0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.5f, 0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        std::vector<uint32_t> indices = {
            0, 2, 1,
            0, 3, 2
        };

        lne::SafePtr<lne::StorageBuffer> vertexBuffer = lne::ApplicationBase::GetRenderer().CreateGeometryBuffer(vertices.data(), vertices.size() * sizeof(Vertex));
        lne::SafePtr<lne::StorageBuffer> indexBuffer = lne::ApplicationBase::GetRenderer().CreateGeometryBuffer(indices.data(), indices.size() * sizeof(uint32_t));
        
        m_Geometry.VertexGPUBuffer = vertexBuffer;
        m_Geometry.VertexCount = (uint32_t)vertices.size();
        m_Geometry.IndexGPUBuffer = indexBuffer;
        m_Geometry.IndexCount = (uint32_t)indices.size();

        m_Transform.Position = { -0.5f, 0.0f, 0.0f };
        m_Transform.Rotation = { 0.0f, 0.0f, 0.0f };
        m_Transform.Scale = { 0.5f, 0.5f, 0.5f };

        m_Transform.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();

        m_Transform2.Position = { 0.5f, 0.0f, 0.0f };
        m_Transform2.Rotation = { 0.0f, 0.0f, 0.0f };
        m_Transform2.Scale = { 0.5f, 0.5f, 0.5f };

        m_Transform2.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();
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

        double currentTime = lne::ApplicationBase::GetClock().GetElapsedTime();
        float sinTime = sin(currentTime);
        float cosTime = cos(currentTime);

        m_Transform.Position.y = sinTime * 0.5f;
        m_Transform2.Rotation.z = cosTime * 180.0f;

        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);

        lne::ApplicationBase::GetRenderer().Draw(m_Pipeline, m_Geometry, m_Transform);
        lne::ApplicationBase::GetRenderer().Draw(m_Pipeline, m_Geometry, m_Transform2);

        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

private:
    lne::SafePtr<lne::GraphicsPipeline> m_Pipeline;
    lne::Geometry m_Geometry;
    lne::TransformComponent m_Transform;
    lne::TransformComponent m_Transform2;
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
        1280, 720,
        true
    });
}
