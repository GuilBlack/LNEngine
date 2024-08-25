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
        desc.Name = "Basic";
        desc.AddStage(lne::ShaderStage::eVertex)
            .AddStage(lne::ShaderStage::eFragment);
        desc.SetCulling(lne::ECullMode::Back)
            .SetWinding(lne::EWindingOrder::CounterClockwise)
            .SetFill(lne::EFillMode::Solid);
        desc.Framebuffer = fb;
        desc.Blend.EnableBlend(false);
        fb.SetClearColor({0.105f, 0.117f, 0.149f, 1.0f });

        m_Pipeline = lne::ApplicationBase::GetRenderer().CreateGraphicsPipeline(desc);

        m_Texture = lne::ApplicationBase::GetRenderer().CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");

        m_BasicMaterial = lnnew lne::Material(m_Pipeline);
        m_BasicMaterial2 = lnnew lne::Material(m_Pipeline);

        m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        m_BasicMaterial->SetTexture("tDiffuse", m_Texture);
        m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));


        struct Vertex {
            glm::vec4 Position;
            glm::vec4 Color;
            glm::vec2 TexCoord;
        };

        std::vector<Vertex> vertices = {
            // Front Face
            {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},

            // Back Face
            {{-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
        };

        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0, //
            1, 5, 6, 6, 2, 1, //
            5, 4, 7, 7, 6, 5, //
            4, 0, 3, 3, 7, 4, //
            3, 2, 6, 6, 7, 3, //
            4, 5, 1, 1, 0, 4 
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
        float sinTime = (float)sin(currentTime);
        float cosTime = (float)cos(currentTime);

        m_Transform.Position.y = sinTime * 0.5f;
        m_Transform2.Rotation.z = cosTime * 180.0f;

        m_BasicMaterial->SetProperty("uColor", glm::vec4((sinTime+1)/2, (cosTime+1.0f)/2, 1.0f, 1.0f));

        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);

        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial, m_Geometry, m_Transform);
        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial2, m_Geometry, m_Transform2);

        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

private:
    lne::SafePtr<lne::GfxPipeline> m_Pipeline;
    lne::SafePtr<lne::Material> m_BasicMaterial;
    lne::SafePtr<lne::Material> m_BasicMaterial2;
    lne::Geometry m_Geometry;
    lne::TransformComponent m_Transform;
    lne::TransformComponent m_Transform2;
    lne::SafePtr<lne::Texture> m_Texture;
};

class Application : public lne::ApplicationBase
{
public:
    Application(lne::ApplicationSettings&& settings)
        : lne::ApplicationBase(std::move(settings))
    {
        PushLayer(lnnew AppLayer());
    }
};

lne::ApplicationBase* lne::CreateApplication()
{
    return lnnew Application({
        "LNApp",
        1280, 720,
        true
    });
}
