#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
struct Vertex
{
    glm::vec4 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
};
public:
    AppLayer()
        : Layer("AppLayer")
    {}
    void OnAttach() override
    {
        APP_INFO("AppLayer::OnAttach");
        lne::ApplicationBase::GetEventHub().RegisterListener<lne::WindowResizeEvent>(this, &AppLayer::OnWindowResize, 10);

        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();
        fb.SetClearColor({0.105f, 0.117f, 0.149f, 1.0f });
        lne::GraphicsPipelineDesc desc{};
        desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders\\HelloTriangle.glsl";
        desc.Name = "Basic";
        desc.EnableDepthTest(true);
        desc.Framebuffer = fb;
        desc.Blend.EnableBlend(false);

        m_BasePipeline = lne::ApplicationBase::GetRenderer().CreateGraphicsPipeline(desc);
        m_BasicMaterial = lnnew lne::Material(m_BasePipeline);
        m_BasicMaterial2 = lnnew lne::Material(m_BasePipeline);
        
        desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders\\Skybox.glsl";
        desc.Name = "Skybox";
        desc.CullMode = lne::ECullMode::None;
        desc.EnableDepthTest(true);

        m_SkyboxPipeline = lne::ApplicationBase::GetRenderer().CreateGraphicsPipeline(desc);
        m_SkyboxMaterial = lnnew lne::Material(m_SkyboxPipeline);

        m_Texture = lne::ApplicationBase::GetRenderer().CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");
        std::string cubemapPath = lne::ApplicationBase::GetAssetsPath() + "Textures\\Skybox\\";
        m_CubemapTexture = lne::ApplicationBase::GetRenderer().CreateCubemapTexture({
            cubemapPath + "px.png",
            cubemapPath + "nx.png",
            cubemapPath + "py.png",
            cubemapPath + "ny.png",
            cubemapPath + "pz.png",
            cubemapPath + "nz.png"
        });

        m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        m_BasicMaterial->SetTexture("tDiffuse", m_Texture);
        m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));
        m_SkyboxMaterial->SetTexture("tDiffuse", m_CubemapTexture);

        std::vector<Vertex> tesselatedVertices{};
        std::vector<uint32_t> tesselatedIndices{};
        GenerateCube(tesselatedVertices, tesselatedIndices, 1);

        lne::SafePtr<lne::StorageBuffer> tesselatedVertexBuffer = lne::ApplicationBase::GetRenderer()
            .CreateGeometryBuffer(tesselatedVertices.data(), tesselatedVertices.size() * sizeof(Vertex));
        lne::SafePtr<lne::StorageBuffer> tesselatedIndexBuffer = lne::ApplicationBase::GetRenderer()
            .CreateGeometryBuffer(tesselatedIndices.data(), tesselatedIndices.size() * sizeof(uint32_t));

        m_TesselatedCubeGeo.VertexGPUBuffer = tesselatedVertexBuffer;
        m_TesselatedCubeGeo.VertexCount = (uint32_t)tesselatedVertices.size();
        m_TesselatedCubeGeo.IndexGPUBuffer = tesselatedIndexBuffer;
        m_TesselatedCubeGeo.IndexCount = (uint32_t)tesselatedIndices.size();

        m_CubeTransform.Position =  { -0.5f, 0.0f, 0.0f };
        m_CubeTransform.Rotation =  { 0.0f, 0.0f, 0.0f };
        m_CubeTransform.Scale =     { 0.25f, 0.25f, 0.25f };

        m_CubeTransform.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();

        m_CubeTransform2.Position = { 0.5f, 0.0f, 0.0f };
        m_CubeTransform2.Rotation = { 0.0f, 0.0f, 0.0f };
        m_CubeTransform2.Scale =    { 0.25f, 0.25f, 0.25f };

        m_CubeTransform2.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();

        m_CameraTransform.Position = { 0.0f, 0.0f, -2.0f };

        m_SkyboxTransform.Position = { 0.0f, 0.0f, 0.0f };
        m_SkyboxTransform.Scale = { .25f, .25f, .25f };

        m_SkyboxTransform.UniformBuffers = lne::ApplicationBase::GetRenderer().RegisterObject();

        m_Camera.LookAtCenter(m_CameraTransform.Position);
        auto& windowSettings = lne::ApplicationBase::GetWindow().GetSettings();
        m_Camera.SetPerspective(45.0f, windowSettings.Width / (float)windowSettings.Height, 0.001f, 10000.0f);
    }

    void OnDetach() override
    {
        APP_INFO("AppLayer::OnDetach");
        m_BasePipeline.Reset();
    }

    void OnUpdate(float deltaTime) override
    {
        HandleInput(deltaTime);
        static int frameIndex = 0;
        ++frameIndex;

        double currentTime = lne::ApplicationBase::GetClock().GetElapsedTime();
        float sinTime = (float)sin(currentTime);
        float cosTime = (float)cos(currentTime);

        m_CubeTransform.Position.y = sinTime * 0.5f;
        m_CubeTransform2.Rotation.z = cosTime * 180.0f;

        m_BasicMaterial->SetProperty("uColor", glm::vec4((sinTime+1)/2, (cosTime+1.0f)/2, 1.0f, 1.0f));

        lne::ApplicationBase::GetRenderer().BeginScene(m_Camera);

        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);

        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial, m_TesselatedCubeGeo, m_CubeTransform);
        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial2, m_TesselatedCubeGeo, m_CubeTransform2);
        lne::ApplicationBase::GetRenderer().Draw(m_SkyboxMaterial, m_TesselatedCubeGeo, m_SkyboxTransform);

        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

    void OnImGuiRender() override
    {
        ImGui::Begin("Hello, world!");
        ImGui::Text("This is some useful text.");
        ImGui::End();
    }

    bool OnWindowResize(lne::WindowResizeEvent& event)
    {
        APP_INFO("WindowResizeEvent received: {0}x{1}", event.GetWidth(), event.GetHeight());
        if (event.GetWidth() == 0 || event.GetHeight() == 0)
            return false;
        m_Camera.SetPerspective(45.0f, event.GetWidth() / (float)event.GetHeight(), 0.001f, 10000.0f);
        return false;
    }

    void HandleInput(float deltaTime)
    {
        auto& inputManager = lne::ApplicationBase::GetInputManager();

        if (inputManager.IsKeyPressed(lne::eKeyW))
        {
            m_CameraTransform.Position += glm::vec3(m_CameraTransform.GetForward() * deltaTime);
        }
        if (inputManager.IsKeyPressed(lne::eKeyS))
        {
            m_CameraTransform.Position -= glm::vec3(m_CameraTransform.GetForward() * deltaTime);
        }
        if (inputManager.IsKeyPressed(lne::eKeyA))
        {
            m_CameraTransform.Position += glm::vec3(m_CameraTransform.GetRight() * deltaTime);
        }
        if (inputManager.IsKeyPressed(lne::eKeyD))
        {
            m_CameraTransform.Position -= glm::vec3(m_CameraTransform.GetRight() * deltaTime);
        }

        glm::vec2 mouseDelta{};
        if (inputManager.IsMouseButtonPressed(lne::eMouseButton0))
        {
            inputManager.GetMouseDelta(mouseDelta.x, mouseDelta.y);
            m_CameraTransform.Rotation.x -= mouseDelta.y * 0.1f;
            m_CameraTransform.Rotation.y -= mouseDelta.x * 0.1f;
        }

        m_Camera.UpdateView(m_CameraTransform);
    }

private:
    lne::SafePtr<lne::GfxPipeline> m_BasePipeline{};
    lne::SafePtr<lne::Material> m_BasicMaterial{};
    lne::SafePtr<lne::Material> m_BasicMaterial2{};

    lne::SafePtr<lne::GfxPipeline> m_SkyboxPipeline{};
    lne::SafePtr<lne::Material> m_SkyboxMaterial{};

    lne::Geometry m_TesselatedCubeGeo;
    lne::SafePtr<lne::Texture> m_Texture{};
    lne::SafePtr<lne::Texture> m_CubemapTexture{};

    lne::TransformComponent m_CubeTransform{};
    lne::TransformComponent m_CubeTransform2{};
    lne::TransformComponent m_SkyboxTransform{};

    lne::CameraComponent m_Camera{};
    lne::TransformComponent m_CameraTransform{};

private:
    void GenerateCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t tesselationLevel)
    {
        float step = 2.0f / tesselationLevel;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

        auto addQuad = [&](glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3)
        {
            uint32_t startIndex = (uint32_t)vertices.size();
            vertices.push_back({ p0, color, {0.0f, 0.0f} });
            vertices.push_back({ p1, color, {1.0f, 0.0f} });
            vertices.push_back({ p2, color, {1.0f, 1.0f} });
            vertices.push_back({ p3, color, {0.0f, 1.0f} });

            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 1);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 3);
        };

        for (uint32_t i = 0; i < tesselationLevel; ++i)
        {
            for (uint32_t j = 0; j < tesselationLevel; ++j)
            {
                float x0 = -1.0f + i * step;
                float x1 = x0 + step;
                float y0 = -1.0f + j * step;
                float y1 = y0 + step;

                // Front face
                addQuad({ x0, y0, 1.0f, 1.0f }, { x1, y0, 1.0f, 1.0f }, { x1, y1, 1.0f, 1.0f }, { x0, y1, 1.0f, 1.0f });
                // Back face
                addQuad({ x1, y0, -1.0f, 1.0f }, { x0, y0, -1.0f, 1.0f }, { x0, y1, -1.0f, 1.0f }, { x1, y1, -1.0f, 1.0f });
                // Left face
                addQuad({ -1.0f, y0, x0, 1.0f }, { -1.0f, y0, x1, 1.0f }, { -1.0f, y1, x1, 1.0f }, { -1.0f, y1, x0, 1.0f });
                // Right face
                addQuad({ 1.0f, y0, x1, 1.0f }, { 1.0f, y0, x0, 1.0f }, { 1.0f, y1, x0, 1.0f }, { 1.0f, y1, x1, 1.0f });
                // Top face
                addQuad({ x0, 1.0f, y1, 1.0f }, { x1, 1.0f, y1, 1.0f }, { x1, 1.0f, y0, 1.0f }, { x0, 1.0f, y0, 1.0f });
                // Bottom face
                addQuad({ x0, -1.0f, y0, 1.0f }, { x1, -1.0f, y0, 1.0f }, { x1, -1.0f, y1, 1.0f }, { x0, -1.0f, y1, 1.0f });
            }
        }
    }
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
