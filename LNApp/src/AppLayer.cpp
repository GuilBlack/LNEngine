#include "pch.h"
#include "AppLayer.h"

void AppLayer::OnAttach()
{
    using namespace lne;
    APP_INFO("AppLayer::OnAttach");
    ApplicationBase::GetEventHub().RegisterListener<WindowResizeEvent>(this, &AppLayer::OnWindowResize, 10);

    Renderer& renderer = ApplicationBase::GetRenderer();

    InitFrameGraph();

    auto& fb = ApplicationBase::GetWindow().GetCurrentFramebuffer();
    fb.SetClearColor({0.105f, 0.117f, 0.149f, 1.0f });
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Shaders\\MeshLighting.glsl";
    desc.Name = "Basic";
    desc.EnableDepthTest(true);
    desc.Framebuffer = fb;
    desc.Blend.EnableBlend(false);

    m_BasePipeline = renderer.CreateGraphicsPipeline(desc);
    m_BasicMaterial = lnnew Material(m_BasePipeline);
    m_BasicMaterial2 = lnnew Material(m_BasePipeline);
    
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Shaders\\Skybox.glsl";
    desc.Name = "Skybox";
    desc.CullMode = ECullMode::None;
    desc.EnableDepthTest(true);

    m_SkyboxPipeline = renderer.CreateGraphicsPipeline(desc);
    SafePtr skyboxMaterial = lnnew Material(m_SkyboxPipeline);

    SafePtr uvChecker = renderer.CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");
    std::string cubemapPath = lne::ApplicationBase::GetAssetsPath() + "Textures\\Skybox\\";
    SafePtr skyboxTexture = renderer.CreateCubemapTexture({
        cubemapPath + "px.png",
        cubemapPath + "nx.png",
        cubemapPath + "py.png",
        cubemapPath + "ny.png",
        cubemapPath + "pz.png",
        cubemapPath + "nz.png"
    });

    m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_BasicMaterial->SetTexture("tAlbedo", uvChecker);
    m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));
    skyboxMaterial->SetTexture("tAlbedo", skyboxTexture);

#pragma region CreateEntities
    m_CameraEntity = m_Scene.CreateEntity();
    CameraComponent& cameraComponent = m_CameraEntity.EmplaceComponent<CameraComponent>();
    TransformComponent& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();

    m_DuckEntity = m_Scene.CreateEntity();
    m_CubeEntity = m_Scene.CreateEntity();
    m_SkyboxEntity = m_Scene.CreateEntity();
    m_SphereEntity = m_Scene.CreateEntity();
    
    m_DuckEntity.EmplaceComponent<StaticMeshComponent>();
    m_CubeEntity.EmplaceComponent<StaticMeshComponent>();
    m_SkyboxEntity.EmplaceComponent<StaticMeshComponent>();
    m_SphereEntity.EmplaceComponent<StaticMeshComponent>();

    auto[duckTransform, duckMeshComponent] = m_DuckEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[cubeTransform, cubeMeshComponent] = m_CubeEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[skyboxTransform, skyboxMeshComponent] = m_SkyboxEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[sphereTransform, sphereMeshComponent] = m_SphereEntity.GetComponents<TransformComponent, StaticMeshComponent>();
#pragma endregion

#pragma region CubeGen
    std::vector<Vertex> tesselatedVertices{};
    std::vector<uint32_t> tesselatedIndices{};
    GenerateCube(tesselatedVertices, tesselatedIndices, 1);

    SafePtr<StorageBuffer> tesselatedVertexBuffer = renderer
        .CreateGeometryBuffer(tesselatedVertices.data(), tesselatedVertices.size() * sizeof(Vertex));
    SafePtr<StorageBuffer> tesselatedIndexBuffer = renderer
        .CreateGeometryBuffer(tesselatedIndices.data(), tesselatedIndices.size() * sizeof(uint32_t));
    Geometry cubeGeo{};
    cubeGeo.VertexGPUBuffer = tesselatedVertexBuffer;
    cubeGeo.VertexCount = (uint32_t)tesselatedVertices.size();
    cubeGeo.IndexGPUBuffer = tesselatedIndexBuffer;
    cubeGeo.IndexCount = (uint32_t)tesselatedIndices.size();
#pragma endregion

#pragma region SphereGen
    std::vector<Vertex> sphereVertices{};
    std::vector<uint32_t> sphereIndices{};
    GenerateUVSphere(sphereVertices, sphereIndices);

    SafePtr<StorageBuffer> sphereVertexBuffer = renderer
        .CreateGeometryBuffer(sphereVertices.data(), sphereVertices.size() * sizeof(Vertex));
    SafePtr<StorageBuffer> sphereIndexBuffer = renderer
        .CreateGeometryBuffer(sphereIndices.data(), sphereIndices.size() * sizeof(uint32_t));

    Geometry sphereGeo{};
    sphereGeo.VertexGPUBuffer = sphereVertexBuffer;
    sphereGeo.VertexCount = (uint32_t)sphereVertices.size();
    sphereGeo.IndexGPUBuffer = sphereIndexBuffer;
    sphereGeo.IndexCount = (uint32_t)sphereIndices.size();
#pragma endregion

#pragma region LoadModels
    duckMeshComponent.Mesh = lnnew StaticMesh(ApplicationBase::GetAssetsPath() + "Models\\gltf\\Models\\Duck\\gltf\\Duck.gltf", m_BasePipeline);
    cubeMeshComponent.Mesh = lnnew StaticMesh(cubeGeo, m_BasicMaterial, { uvChecker }, m_BasePipeline);
    skyboxMeshComponent.Mesh = lnnew StaticMesh(sphereGeo, skyboxMaterial, { skyboxTexture }, m_SkyboxPipeline);
    sphereMeshComponent.Mesh = lnnew StaticMesh(sphereGeo, m_BasicMaterial2, { uvChecker }, m_BasePipeline);
#pragma endregion

#pragma region TransformInit
    cubeTransform.Position =  { -0.5f, 0.0f, 0.0f };
    cubeTransform.Scale =     { 0.25f, 0.25f, 0.25f };

    cubeTransform.UniformBuffers = renderer.RegisterObject();

    sphereTransform.Position = { 0.5f, 0.0f, 0.0f };
    sphereTransform.Scale =    { 0.25f, 0.25f, 0.25f };

    sphereTransform.UniformBuffers = renderer.RegisterObject();

    skyboxTransform.Position = { 0.0f, 0.0f, 0.0f };
    skyboxTransform.Scale = { 1.f, 1.f, 1.f };

    skyboxTransform.UniformBuffers = renderer.RegisterObject();

    duckTransform.Position = { 0.0f, 0.0f, 0.0f };
    duckTransform.Scale = { .2f, .2f, .2f };

    duckTransform.UniformBuffers = renderer.RegisterObject();
#pragma endregion

    cameraTransform.Position = { 0.0f, 0.0f, 2.0f };
    cameraTransform.LookAt({ 0.0f, 0.0f, 0.0f });

    auto& windowSettings = ApplicationBase::GetWindow().GetSettings();
    cameraComponent.SetPerspective(45.0f, windowSettings.Width / (float)windowSettings.Height, 0.001f, 10000.0f);

    m_CameraTarget.Position = cameraTransform.Position;
    m_CameraTarget.Rotation = cameraTransform.EulerAngles;
    cameraComponent.UpdateView(cameraTransform);
}

void AppLayer::InitFrameGraph()
{
    auto [width, height] = lne::ApplicationBase::GetWindow().GetSwapchain()->GetViewport().GetExtent();

    lne::FrameGraphResourceDescBuilder resourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphNodeDescBuilder nodeBuilder = lne::FrameGraphNodeDescBuilder();

    lne::FrameGraphResourceDesc colorAttachmentDesc = resourceBuilder.SetName("color")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageDimension(width, height)
        .Build();
    lne::FrameGraphResourceDesc metRoughOccAttachmentDesc = resourceBuilder.SetName("metallic_roughness_occlusion")
        .Build();
    lne::FrameGraphResourceDesc normalAttachmentDesc = resourceBuilder.SetName("normal")
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .Build();
    lne::FrameGraphResourceDesc positionAttachmentDesc = resourceBuilder.SetName("position")
        .Build();
    lne::FrameGraphResourceDesc depthAttachmentDesc = resourceBuilder.SetDefaultDepthAttachmentInfos()
        .SetName("Depth").Build();

    lne::FrameGraphNodeDesc gBufferPassDesc = nodeBuilder.SetName("GBufferPass")
        .AddInputResource(depthAttachmentDesc)
        .AddOutputResource(metRoughOccAttachmentDesc)
        .AddOutputResource(normalAttachmentDesc)
        .AddOutputResource(positionAttachmentDesc)
        .AddOutputResource(colorAttachmentDesc)
        .Build();
    nodeBuilder.Clear();
    metRoughOccAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    normalAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    positionAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    colorAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    lne::FrameGraphResourceDesc lightingResource = resourceBuilder.SetName("Lighting")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .Build();
    lne::FrameGraphNodeDesc lightingPassDesc = nodeBuilder.SetName("LightingPass")
        .AddInputResource(metRoughOccAttachmentDesc)
        .AddInputResource(normalAttachmentDesc)
        .AddInputResource(positionAttachmentDesc)
        .AddInputResource(colorAttachmentDesc)
        .AddOutputResource(lightingResource)
        .Build();
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc depthPrePassDesc = nodeBuilder.SetName("DepthPrePass")
        .AddOutputResource(depthAttachmentDesc)
        .Build();
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc dofPass = nodeBuilder.SetName("DoFPass")
        .AddInputResource(resourceBuilder.SetName("Lighting_Ref")
            .SetType(lne::FrameGraphResourceType::eProxy)
            .SetProxyInfo("Lighting")
            .Build()
        )
        .AddOutputResource(resourceBuilder.SetName("Final")
            .SetType(lne::FrameGraphResourceType::eAttachment)
            .SetDefaultColorAttachmentInfos()
            .Build()
        )
        .Build();
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc transparentPass = nodeBuilder.SetName("TransparentPass")
        .AddInputResource(depthAttachmentDesc)
        .AddInputResource(lightingResource)
        .AddOutputResource(resourceBuilder.SetName("Lighting_Ref")
            .SetType(lne::FrameGraphResourceType::eProxy)
            .SetProxyInfo("Lighting")
            .Build()
        )
        .Build();

    m_FrameGraphTest.CreateNode(gBufferPassDesc);
    m_FrameGraphTest.CreateNode(lightingPassDesc);
    m_FrameGraphTest.CreateNode(depthPrePassDesc);
    m_FrameGraphTest.CreateNode(dofPass);
    m_FrameGraphTest.CreateNode(transparentPass);

    m_FrameGraphTest.Compile();

    m_FrameGraphTest.BindRenderPass(lnnew DepthPrePass());
    m_FrameGraphTest.BindRenderPass(lnnew GBufferPass());
    m_FrameGraphTest.BindRenderPass(lnnew LightingPass());
    m_FrameGraphTest.BindRenderPass(lnnew DoFPass());
    m_FrameGraphTest.BindRenderPass(lnnew TransparentPass());
}

void AppLayer::OnDetach()
{
    APP_INFO("AppLayer::OnDetach");
    m_BasePipeline.Reset();
}

void AppLayer::OnUpdate(float deltaTime)
{
    m_Scene.BeginScene();
    double currentTime = lne::ApplicationBase::GetClock().GetElapsedTime();
    HandleInput(deltaTime);
    static int frameIndex = 0;
    ++frameIndex;

    float sinTime = (float)sin(currentTime);
    float cosTime = (float)cos(currentTime);

    m_CubeEntity.GetComponent<lne::TransformComponent>().Position.y = sinTime * 0.5f;

    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();

    renderer.BeginScene(m_CameraEntity.GetComponent<lne::TransformComponent>(), m_CameraEntity.GetComponent<lne::CameraComponent>(), m_LightDirection);

    auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

    renderer.BeginRenderPass(fb);

    renderer.Draw(m_CubeEntity.GetComponent<lne::StaticMeshComponent>().Mesh, m_CubeEntity.GetComponent<lne::TransformComponent>());
    renderer.Draw(m_SphereEntity.GetComponent<lne::StaticMeshComponent>().Mesh, m_SphereEntity.GetComponent<lne::TransformComponent>());
    if (m_DuckEntity.IsValid())
        renderer.Draw(m_DuckEntity.GetComponent<lne::StaticMeshComponent>().Mesh, m_DuckEntity.GetComponent<lne::TransformComponent>());
    renderer.Draw(m_SkyboxEntity.GetComponent<lne::StaticMeshComponent>().Mesh, m_SkyboxEntity.GetComponent<lne::TransformComponent>());

    renderer.EndRenderPass(fb);

    m_FrameGraphTest.Execute(renderer.GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer());
    m_Scene.EndScene();
}

void AppLayer::OnImGuiRender()
{
    ImGui::Begin("Hello, world!");
        
    if (ImGui::SliderFloat("Metalness", &m_Metalness, 0.0f, 1.0f))
    { 
        m_BasicMaterial->SetProperty("uMetalness", m_Metalness);
        m_BasicMaterial2->SetProperty("uMetalness", m_Metalness);
    }
        
    if (ImGui::SliderFloat("Roughness", &m_Roughness, 0.0f, 1.0f))
    {
        m_BasicMaterial->SetProperty("uRoughness", m_Roughness);
        m_BasicMaterial2->SetProperty("uRoughness", m_Roughness);
    }

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

    if (ImGui::Button("Delete Duck"))
    {
        m_Scene.DestroyEntity(m_DuckEntity);
        m_DuckEntity = lne::Entity{};
    }

    ImGui::Text("This is some useful text.");
    ImGui::End();
}

bool AppLayer::OnWindowResize(lne::WindowResizeEvent& event) 
{
    APP_INFO("WindowResizeEvent received: {0}x{1}", event.GetWidth(), event.GetHeight());
    if (event.GetWidth() == 0 || event.GetHeight() == 0)
        return false;
    m_CameraEntity.GetComponent<lne::CameraComponent>().SetPerspective(45.0f, event.GetWidth() / (float)event.GetHeight(), 0.001f, 10000.0f);
    return false;
}

void AppLayer::HandleInput(float deltaTime)
{
    auto& inputManager = lne::ApplicationBase::GetInputManager();

    float movementSpeed = 1.0f;
    float rotationSpeed = 0.1f;


    if (inputManager.IsKeyPressed(lne::eKeyLeftShift) || inputManager.IsKeyPressed(lne::eKeyRightShift))
        movementSpeed *= 10.0f;
    if (inputManager.IsKeyPressed(lne::eKeyLeftControl) || inputManager.IsKeyPressed(lne::eKeyRightControl))
        movementSpeed *= 0.1f;
    auto& camTransform = m_CameraEntity.GetComponent<lne::TransformComponent>();
    auto& cam = m_CameraEntity.GetComponent<lne::CameraComponent>();
    glm::vec3 movementInput{ 0.0f };
    if (inputManager.IsKeyPressed(lne::eKeyW))
        movementInput += camTransform.GetForward();
    if (inputManager.IsKeyPressed(lne::eKeyS))
        movementInput -= camTransform.GetForward();
    if (inputManager.IsKeyPressed(lne::eKeyD))
        movementInput += camTransform.GetRight();
    if (inputManager.IsKeyPressed(lne::eKeyA))
        movementInput -= camTransform.GetRight();
    if (inputManager.IsKeyPressed(lne::eKeyQ))
        movementInput += camTransform.GetUp();
    if (inputManager.IsKeyPressed(lne::eKeyE))
        movementInput -= camTransform.GetUp();

    if (glm::length(movementInput) > 0.0f)
    {
        movementInput = glm::normalize(movementInput);
        m_CameraTarget.Position += movementInput * movementSpeed * deltaTime;
    }

    glm::vec2 mouseDelta{};
    if (inputManager.IsMouseButtonPressed(lne::eMouseButton0))
    {
        inputManager.GetMouseDelta(mouseDelta.x, mouseDelta.y);
        m_CameraTarget.Rotation.x -= mouseDelta.y * rotationSpeed;
        m_CameraTarget.Rotation.y -= mouseDelta.x * rotationSpeed;
    }

    // Clamp pitch to avoid gimbal lock
    const float maxPitch = 89.9f;
    m_CameraTarget.Rotation.x = glm::clamp(m_CameraTarget.Rotation.x, -maxPitch, maxPitch);

    // Smoothly interpolate actual position and rotation towards target values
    float positionLerpFactor = 0.99f;
    float rotationLerpFactor = 0.99f;

    camTransform.Position = Lerp3(camTransform.Position, m_CameraTarget.Position, positionLerpFactor, deltaTime);
    camTransform.SetEulerAngles(Lerp3(camTransform.EulerAngles, m_CameraTarget.Rotation, rotationLerpFactor, deltaTime));
    cam.UpdateView(camTransform);
}

void AppLayer::GenerateCube(std::vector<lne::Vertex>& vertices, std::vector<uint32_t>& indices,
    uint32_t tesselationLevel)
{
    float step = 2.0f / tesselationLevel;

    auto addQuad = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal)
    {
        uint32_t startIndex = (uint32_t)vertices.size();
        vertices.push_back({ p0, normal, {0.0f, 0.0f} });
        vertices.push_back({ p1, normal, {1.0f, 0.0f} });
        vertices.push_back({ p2, normal, {1.0f, 1.0f} });
        vertices.push_back({ p3, normal, {0.0f, 1.0f} });

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
            addQuad({ x0, y0, 1.0f }, { x1, y0, 1.0f }, { x1, y1, 1.0f }, { x0, y1, 1.0f }, { 0.0f, 0.0f, 1.0f });
            // Back face
            addQuad({ x1, y0, -1.0f }, { x0, y0, -1.0f }, { x0, y1, -1.0f }, { x1, y1, -1.0f }, { 0.0f, 0.0f, -1.0f });
            // Left face
            addQuad({ -1.0f, y0, x0 }, { -1.0f, y0, x1 }, { -1.0f, y1, x1 }, { -1.0f, y1, x0 }, { -1.0f, 0.0f, 0.0f });
            // Right face
            addQuad({ 1.0f, y0, x1 }, { 1.0f, y0, x0 }, { 1.0f, y1, x0 }, { 1.0f, y1, x1 }, { 1.0f, 0.0f, 0.0f });
            // Top face
            addQuad({ x0, 1.0f, y0 }, { x0, 1.0f, y1 }, { x1, 1.0f, y1 }, { x1, 1.0f, y0 }, { 0.0f, 1.0f, 0.0f });
            // Bottom face
            addQuad({ x0, -1.0f, y0 }, { x1, -1.0f, y0 }, { x1, -1.0f, y1 }, { x0, -1.0f, y1 }, { 0.0f, -1.0f, 0.0f });
        }
    }
}

void AppLayer::GenerateUVSphere(std::vector<lne::Vertex>& vertices, std::vector<uint32_t>& indices, float radius,
    uint32_t nLatitude, uint32_t nLongitude)
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
        vertices[count].Position = { 0.0f, radius, 0.0f };
        vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 0.0f };
        vertices[count].Normal = { 0.0f, 1.0f, 0.0f };
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

            vertices[count].Position = { radius * point.x, radius * point.y, radius * point.z };
            vertices[count].TexCoord = { (float)j / (float)nLongitude, (float)i / (float)(nLatitude + 1) };
            vertices[count].Normal = glm::vec3(point);

            ++count;
        }
    }

    //add south pole
    for (uint32_t i = 1; i <= nLongitude; ++i)
    {
        vertices[count].Position = { 0.0f, -radius, 0.0f };
        vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 1.0f };
        vertices[count].Normal = { 0.0f, -1.0f, 0.0f };
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
