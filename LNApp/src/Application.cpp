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
            // Front face
            {{-1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
            {{ 1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
            {{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},  // Top-right
            {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // Top-left
            // Back face
            {{ 1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
            {{-1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
            {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},  // Top-right
            {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},  // Top-left
            // Left face
            {{-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
            {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
            {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // Top-right
            {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}},  // Top-left
            // Right face
            {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
            {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
            {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},  // Top-right
            {{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}},  // Top-left
            // Top face
            {{-1.0f,  1.0f,  1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},  // Bottom-left
            {{ 1.0f,  1.0f,  1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},  // Bottom-right
            {{ 1.0f,  1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},  // Top-right
            {{-1.0f,  1.0f, -1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {0.0f, 0.0f}},  // Top-left
            // Bottom face
            {{-1.0f, -1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},  // Bottom-left
            {{ 1.0f, -1.0f, -1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},  // Bottom-right
            {{ 1.0f, -1.0f,  1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // Top-right
            {{-1.0f, -1.0f,  1.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}},  // Top-left
        };

        std::vector<uint32_t> indices = {
            // Front face
            0, 2, 1, 2, 0, 3,
            // Back face
            4, 6, 5, 6, 4, 7,
            // Left face
            8, 10, 9, 10, 8, 11,
            // Right face
            12, 14, 13, 14, 12, 15,
            // Top face
            16, 18, 17, 18, 16, 19,
            // Bottom face
            20, 22, 21, 22, 20, 23
        };

        lne::SafePtr<lne::StorageBuffer> vertexBuffer = lne::ApplicationBase::GetRenderer().CreateGeometryBuffer(vertices.data(), vertices.size() * sizeof(Vertex));
        lne::SafePtr<lne::StorageBuffer> indexBuffer = lne::ApplicationBase::GetRenderer().CreateGeometryBuffer(indices.data(), indices.size() * sizeof(uint32_t));
        
        m_Geometry.VertexGPUBuffer = vertexBuffer;
        m_Geometry.VertexCount = (uint32_t)vertices.size();
        m_Geometry.IndexGPUBuffer = indexBuffer;
        m_Geometry.IndexCount = (uint32_t)indices.size();

        m_Transform.Position = { -0.5f, 0.0f, 0.0f };
        m_Transform.Rotation = { 0.0f, 0.0f, 0.0f };
        m_Transform.Scale = { 0.25f, 0.25f, 0.25f };

        m_Transform.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();

        m_Transform2.Position = { 0.5f, 0.0f, 0.0f };
        m_Transform2.Rotation = { 0.0f, 0.0f, 0.0f };
        m_Transform2.Scale = { 0.25f, 0.25f, 0.25f };

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

    void OnImGuiRender() override
    {
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::End();
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
