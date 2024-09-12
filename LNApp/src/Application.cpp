#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
struct Vertex
{
    glm::vec4 Position{};
    glm::vec4 Normal{};
    glm::vec2 TexCoord{};
    glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
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
        desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders\\MeshLighting.glsl";
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
        m_BasicMaterial->SetTexture("tAlbedo", m_Texture);
        m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));
        m_SkyboxMaterial->SetTexture("tAlbedo", m_CubemapTexture);

    #pragma region CubeGen
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
    #pragma endregion

    #pragma region SphereGen
        std::vector<Vertex> sphereVertices{};
        std::vector<uint32_t> sphereIndices{};
        GenerateUVSphere(sphereVertices, sphereIndices);

        lne::SafePtr<lne::StorageBuffer> sphereVertexBuffer = lne::ApplicationBase::GetRenderer()
            .CreateGeometryBuffer(sphereVertices.data(), sphereVertices.size() * sizeof(Vertex));
        lne::SafePtr<lne::StorageBuffer> sphereIndexBuffer = lne::ApplicationBase::GetRenderer()
            .CreateGeometryBuffer(sphereIndices.data(), sphereIndices.size() * sizeof(uint32_t));

        m_SphereGeo.VertexGPUBuffer = sphereVertexBuffer;
        m_SphereGeo.VertexCount = (uint32_t)sphereVertices.size();
        m_SphereGeo.IndexGPUBuffer = sphereIndexBuffer;
        m_SphereGeo.IndexCount = (uint32_t)sphereIndices.size();
    #pragma endregion

    #pragma region TransformInit
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
    #pragma endregion

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
        //m_CubeTransform2.Rotation.z = cosTime * 180.0f;

        m_BasicMaterial->SetProperty("uMetalness", 0.04f);
        m_BasicMaterial->SetProperty("uRoughness", 0.05f);

        lne::ApplicationBase::GetRenderer().BeginScene(m_CameraTransform, m_Camera, m_LightDirection);

        auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

        lne::ApplicationBase::GetRenderer().BeginRenderPass(fb);

        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial, m_TesselatedCubeGeo, m_CubeTransform);
        lne::ApplicationBase::GetRenderer().Draw(m_BasicMaterial2, m_SphereGeo, m_CubeTransform2);
        //lne::ApplicationBase::GetRenderer().Draw(m_SkyboxMaterial, m_TesselatedCubeGeo, m_SkyboxTransform);

        lne::ApplicationBase::GetRenderer().EndRenderPass(fb);
    }

    void OnImGuiRender() override
    {
        ImGui::Begin("Hello, world!");
        
        if (ImGui::SliderFloat("Metalness", &m_Metalness, 0.0f, 1.0f))
            m_BasicMaterial->SetProperty("uMetalness", m_Metalness);
        
        if (ImGui::SliderFloat("Roughness", &m_Roughness, 0.0f, 1.0f))
            m_BasicMaterial->SetProperty("uRoughness", m_Roughness);

        ImGui::Text("Sun Dir"); 
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float availableWidth = avail.x;
        float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
        float labelWidth = ImGui::CalcTextSize("Z").x; // Width of the "Z" label
        float inputWidth = (availableWidth - itemSpacing * 3 - labelWidth * 3) / 3.0f;

        ImGui::PushItemWidth(inputWidth);
            ImGui::DragFloat("X", &m_LightDirection.x, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");
            ImGui::SameLine();  // This will keep the next input on the same line
            ImGui::DragFloat("Y", &m_LightDirection.y, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");
            ImGui::SameLine();  // This will keep the next input on the same line
            ImGui::DragFloat("Z", &m_LightDirection.z, 0.1f, -FLT_MAX, FLT_MAX, "%.3f");
        ImGui::PopItemWidth(); // Restore the previous item width

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
    lne::Geometry m_SphereGeo;
    lne::SafePtr<lne::Texture> m_Texture{};
    lne::SafePtr<lne::Texture> m_CubemapTexture{};

    lne::TransformComponent m_CubeTransform{};
    lne::TransformComponent m_CubeTransform2{};
    lne::TransformComponent m_SkyboxTransform{};

    lne::CameraComponent m_Camera{};
    lne::TransformComponent m_CameraTransform{};

    glm::vec3 m_LightDirection{ -1.0f, 1.0f, -1.0f };
    float m_Metalness{ 0.0f };
    float m_Roughness{ 0.0f };

private:
    void GenerateCube(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t tesselationLevel)
    {
        float step = 2.0f / tesselationLevel;

        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

        auto addQuad = [&](glm::vec4 p0, glm::vec4 p1, glm::vec4 p2, glm::vec4 p3, glm::vec4 normal)
        {
            uint32_t startIndex = (uint32_t)vertices.size();
            vertices.push_back({ p0, normal, {0.0f, 0.0f}, color });
            vertices.push_back({ p1, normal, {1.0f, 0.0f}, color });
            vertices.push_back({ p2, normal, {1.0f, 1.0f}, color });
            vertices.push_back({ p3, normal, {0.0f, 1.0f}, color });

            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 1);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 3);
            indices.push_back(startIndex + 0);
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
                addQuad({ x0, y0, 1.0f, 1.0f }, { x1, y0, 1.0f, 1.0f }, { x1, y1, 1.0f, 1.0f }, { x0, y1, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f });
                // Back face
                addQuad({ x1, y0, -1.0f, 1.0f }, { x0, y0, -1.0f, 1.0f }, { x0, y1, -1.0f, 1.0f }, { x1, y1, -1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f, 1.0f });
                // Left face
                addQuad({ -1.0f, y0, x0, 1.0f }, { -1.0f, y0, x1, 1.0f }, { -1.0f, y1, x1, 1.0f }, { -1.0f, y1, x0, 1.0f }, { -1.0f, 0.0f, 0.0f, 1.0f });
                // Right face
                addQuad({ 1.0f, y0, x1, 1.0f }, { 1.0f, y0, x0, 1.0f }, { 1.0f, y1, x0, 1.0f }, { 1.0f, y1, x1, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
                // Top face
                addQuad({ x0, 1.0f, y1, 1.0f }, { x1, 1.0f, y1, 1.0f }, { x1, 1.0f, y0, 1.0f }, { x0, 1.0f, y0, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f });
                // Bottom face
                addQuad({ x0, -1.0f, y0, 1.0f }, { x1, -1.0f, y0, 1.0f }, { x1, -1.0f, y1, 1.0f }, { x0, -1.0f, y1, 1.0f }, { 0.0f, -1.0f, 0.0f, 1.0f });
            }
        }
    }

    void GenerateUVSphere(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, float radius = 1.f, uint32_t nLatitude = 32, uint32_t nLongitude = 32)
    {
        if (nLatitude < 1)
            nLatitude = 1;
        if (nLongitude < 3)
            nLongitude = 3;

        uint32_t nVertices = nLatitude * (nLongitude + 1) + (nLongitude * 2);
        //-1 to nLat because it wouldn't make sense otherwise.
        uint32_t nIndices = 2 * 3 * nLongitude + 2 * 3 * (nLatitude - 1) * nLongitude;

        vertices.resize(nVertices);
        indices.resize(nIndices);

        // here, latitude points should be mapped between -90 and 90 degrees (or -PI/2 to PI/2).
        // +1 to nLat because it wouldn't make sense otherwise.
        float latitudeSlope = glm::pi<float>() / (float)(nLatitude + 1);
        // here, longitude points should be mapped between -180 and 180 degrees (or -PI to PI).
        float longitudeSlope = (2.f * glm::pi<float>()) / (float)nLongitude;

        uint32_t count = 0;
        // add north pole
        for (uint32_t i = 1; i <= nLongitude; ++i)
        {
            vertices[count].Position = { 0.0f, radius, 0.0f, 1.0f };
            vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 0.0f };
            vertices[count].Normal = { 0.0f, 1.0f, 0.0f, 1.0f };
            ++count;
        }

        //middle quads
        for (uint32_t i = 1; i < (nLatitude + 1); ++i)
        {
            float pLat = latitudeSlope * (float)i;
            for (uint32_t j = 0; j < nLongitude + 1; ++j)
            {
                float pLon = longitudeSlope * (float)j;
                glm::vec3 point = { sinf(pLat) * cosf(pLon), cosf(pLat), sinf(pLat) * sinf(pLon) };

                vertices[count].Position = { radius * point.x, radius * point.y, radius * point.z, 1.0f };
                vertices[count].TexCoord = { (float)j / (float)nLongitude, (float)i / (float)(nLatitude + 1) };
                vertices[count].Normal = glm::vec4(point, 1.0f);

                ++count;
            }
        }

        //add south pole
        for (uint32_t i = 1; i <= nLongitude; ++i)
        {
            vertices[count].Position = { 0.0f, -radius, 0.0f, 1.0f };
            vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 1.0f };
            vertices[count].Normal = { 0.0f, -1.0f, 0.0f, 1.0f };
            ++count;
        }

        count = 0;
        //north pole indices
        for (uint32_t i = 0; i < nLongitude; ++i)
        {
            indices[count++] = i;
            indices[count++] = (nLongitude - 1) + i + 2;
            indices[count++] = (nLongitude - 1) + i + 1;
        }

        //middle quads
        for (uint32_t i = 0; i < nLatitude - 1; ++i)
        {
            for (uint32_t j = 0; j < nLongitude; ++j)
            {
                uint32_t index[4] = {
                    nLongitude + i * (nLongitude + 1) + j,
                    nLongitude + i * (nLongitude + 1) + (j + 1),
                    nLongitude + (i + 1) * (nLongitude + 1) + (j + 1),
                    nLongitude + (i + 1) * (nLongitude + 1) + j
                };

                indices[count++] = index[0];
                indices[count++] = index[1];
                indices[count++] = index[2];

                indices[count++] = index[0];
                indices[count++] = index[2];
                indices[count++] = index[3];
            }
        }

        //south pole indices
        const uint32_t southPoleIndex = nVertices - nLongitude;
        for (uint32_t i = 0; i < nLongitude; ++i)
        {
            indices[count++] = southPoleIndex + i;
            indices[count++] = southPoleIndex - (nLongitude + 1) + i;
            indices[count++] = southPoleIndex - (nLongitude + 1) + i + 1;
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
