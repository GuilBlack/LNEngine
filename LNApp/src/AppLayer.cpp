#include "pch.h"
#include "AppLayer.h"

AppLayer::FinalPass::FinalPass()
{
    using namespace lne;
    m_Name = "FinalPass";
    ComputePipelineDesc desc{};
    desc.Name = "Test";
    desc.PathToShader = ApplicationBase::GetAssetsPath() + "Shaders\\Test.comp";
    auto context = ApplicationBase::GetRenderer().GetGfxContext();
    m_Pipeline = lnnew ComputePipeline(context, desc);
    m_Program = lnnew ComputeProgram(m_Pipeline);
    m_OutputTexture = Texture::CreateColorAttachmentTexture(context, 1920, 1080, vk::Format::eR8G8B8A8Unorm, TextureUsageType::eSampledAndStorage, "Test");

    m_Program->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_Program->SetTexture("tOutput", m_OutputTexture);

    m_Program->Dispatch((1920 + 16) / 16, (1080 + 16) / 16, 1);
}

void AppLayer::FinalPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer,
    lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    m_OutputTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    lne::Framebuffer& swapchainFramebuffer = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();
    auto renderTexture = swapchainFramebuffer.GetColorAttachments()[0].Texture;
    lne::FrameGraphResource* colorResource = frameGraph->GetResource("Color");
    if (colorResource == nullptr)
    {
        APP_WARN("FinalPass::Render: Color resource not found");
        return;
    }
    lne::SafePtr<lne::Texture> colorTexture = colorResource->Resource.GetAs<lne::Texture>();
    renderer.Blit(cmdBuffer, colorTexture, renderTexture);
}

void AppLayer::OnAttach()
{
    using namespace lne;
    APP_INFO("AppLayer::OnAttach");
    ApplicationBase::GetEventHub().RegisterListener<WindowResizeEvent>(this, &AppLayer::OnWindowResize, 10);

    Renderer& renderer = ApplicationBase::GetRenderer();
    m_Scene = lnnew HierarchicalScene();
    InitTestFrameGraph();
    InitFrameGraph();
    m_WorldRenderer = lnnew WorldRenderer(m_FrameGraph);

    auto& fb = ApplicationBase::GetWindow().GetCurrentFramebuffer();
    fb.SetClearColor({0.105f, 0.117f, 0.149f, 1.0f });
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\MeshLighting.glsl";
    desc.Name = "Basic";
    desc.FrameGraph = m_FrameGraph.GetPtr();
    desc.EnableDepthTest(true, true);
    desc.Blend.EnableBlend(false);

    m_BasePipeline = renderer.CreateGraphicsPipeline(desc);
    m_BasicMaterial = lnnew Material(m_BasePipeline);
    m_BasicMaterial2 = lnnew Material(m_BasePipeline);
    
    SafePtr uvChecker = renderer.CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");

    m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_BasicMaterial->SetTexture("tAlbedo", uvChecker);
    m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));

#pragma region CreateEntities
    m_CameraEntity = m_Scene->CreateEntity();
    CameraComponent& cameraComponent = m_CameraEntity.EmplaceComponent<CameraComponent>();
    TransformComponent& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();

    m_DuckEntity = m_Scene->CreateEntity();
    m_CubeEntity = m_Scene->CreateEntity();
    m_SphereEntity = m_Scene->CreateEntity();
    
    m_DuckEntity.EmplaceComponent<StaticMeshComponent>();
    m_CubeEntity.EmplaceComponent<StaticMeshComponent>();
    m_SphereEntity.EmplaceComponent<StaticMeshComponent>();

    auto[duckTransform, duckMeshComponent] = m_DuckEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[cubeTransform, cubeMeshComponent] = m_CubeEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[sphereTransform, sphereMeshComponent] = m_SphereEntity.GetComponents<TransformComponent, StaticMeshComponent>();
#pragma endregion

#pragma region Geometry Gen
    std::vector<Vertex> tesselatedVertices{};
    std::vector<uint32_t> tesselatedIndices{};
    Geometry cubeGeo = Geometry::GenerateCube(1);

    std::vector<Vertex> sphereVertices{};
    std::vector<uint32_t> sphereIndices{};
    Geometry sphereGeo = Geometry::GenerateUVSphere(1.0f, 32, 32);
#pragma endregion

#pragma region LoadModels
    duckMeshComponent.Mesh = lnnew StaticMesh(ApplicationBase::GetAssetsPath() + "Models\\gltf\\Models\\Duck\\gltf\\Duck.gltf", m_BasePipeline);
    cubeMeshComponent.Mesh = lnnew StaticMesh(cubeGeo, m_BasicMaterial, { uvChecker }, m_BasePipeline);
    sphereMeshComponent.Mesh = lnnew StaticMesh(sphereGeo, m_BasicMaterial, { uvChecker }, m_BasePipeline);
#pragma endregion

#pragma region TransformInit
    cubeTransform.Position =  { -0.5f, 0.0f, 0.0f };
    cubeTransform.Scale =     { 0.25f, 0.25f, 0.25f };

    sphereTransform.Position = { 0.5f, 0.0f, 0.0f };
    sphereTransform.Scale =    { 0.25f, 0.25f, 0.25f };

    duckTransform.Position = { 0.0f, 0.0f, 0.0f };
    duckTransform.Scale = { 4.f, 4.f, 4.f };

#pragma endregion
    //SafePtr<StaticMesh> purpleSphere = sphereMeshComponent.Mesh;
    //SafePtr<StaticMesh> uvSphere = lnnew StaticMesh(sphereGeo, m_BasicMaterial, { uvChecker }, m_BasePipeline);

    //for (int i = 0; i < 32000; i++)
    //{
    //    Entity temp = m_Scene->CreateEntity();
    //    temp.EmplaceComponent<StaticMeshComponent>();

    //    auto [tempTransform, tempMeshComp] =
    //        temp.GetComponents<TransformComponent, StaticMeshComponent>();

    //    float xRand = (rand() / (float)RAND_MAX) * 60;
    //    float yRand = (rand() / (float)RAND_MAX) * 60;
    //    float zRand = (rand() / (float)RAND_MAX) * 60;
    //    tempTransform.Position = { xRand, yRand, -zRand };
    //    tempTransform.Scale = { 0.25f, 0.25f, 0.25f };
    //    if (i < 16000)
    //        tempMeshComp.Mesh = purpleSphere;
    //    else
    //        tempMeshComp.Mesh = uvSphere;
    //}

    cameraTransform.Position = { 0.0f, 0.0f, 2.0f };
    cameraTransform.LookAt({ 0.0f, 0.0f, 0.0f });

    auto& windowSettings = ApplicationBase::GetWindow().GetSettings();
    cameraComponent.SetPerspective(45.0f, windowSettings.Width / (float)windowSettings.Height, 0.001f, 10000.0f);

    m_CameraTarget.Position = cameraTransform.Position;
    m_CameraTarget.Rotation = cameraTransform.EulerAngles;
    cameraComponent.UpdateView(cameraTransform);
}

void AppLayer::InitTestFrameGraph()
{
    auto [width, height] = lne::ApplicationBase::GetWindow().GetSwapchain()->GetViewport().GetExtent();

    lne::FrameGraphResourceDescBuilder resourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphNodeDescBuilder nodeBuilder = lne::FrameGraphNodeDescBuilder();

    lne::FrameGraphResourceDesc colorAttachmentDesc = resourceBuilder.SetName("Color")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageDimension(width, height)
        .Build();
    lne::FrameGraphResourceDesc metRoughOccAttachmentDesc = resourceBuilder.SetName("MetallicRoughnessOcclusion")
        .Build();
    lne::FrameGraphResourceDesc normalAttachmentDesc = resourceBuilder.SetName("Normal")
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .Build();
    lne::FrameGraphResourceDesc positionAttachmentDesc = resourceBuilder.SetName("Position")
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

    m_FrameGraphTest.BindRenderPass(lnnew GBufferPass());
    m_FrameGraphTest.BindRenderPass(lnnew LightingPass());
    m_FrameGraphTest.BindRenderPass(lnnew DoFPass());
    m_FrameGraphTest.BindRenderPass(lnnew TransparentPass());
}

void AppLayer::InitFrameGraph()
{
    auto [width, height] = lne::ApplicationBase::GetWindow().GetSwapchain()->GetViewport().GetExtent();
    m_FrameGraph = lnnew lne::FrameGraph("BasicFrameGraph");
    lne::FrameGraphResourceDescBuilder resourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphNodeDescBuilder nodeBuilder = lne::FrameGraphNodeDescBuilder();

    lne::FrameGraphResourceDesc colorAttachmentDesc = resourceBuilder.SetName("Color")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageDimension(width, height)
        .Build();

    lne::FrameGraphResourceDesc depthAttachmentDesc = resourceBuilder.SetDefaultDepthAttachmentInfos()
        .SetName("Depth").Build();

    lne::FrameGraphNodeDesc offscreenPassDesc = nodeBuilder.SetName("BasicForwardPass")
        .AddOutputResource(colorAttachmentDesc)
        .AddInputResource(depthAttachmentDesc)
        .Build();

    nodeBuilder.Clear();

    lne::FrameGraphNodeDesc depthPrePassDesc = nodeBuilder.SetName("DepthPrePass")
        .AddOutputResource(depthAttachmentDesc)
        .Build();

    nodeBuilder.Clear();

    lne::FrameGraphNodeDesc finalPassDesc = nodeBuilder.SetName("FinalPass")
        .AddInputResource(colorAttachmentDesc)
        .SetType(lne::RenderPassType::eTransfer)
        .Build();

    m_FrameGraph->CreateNode(finalPassDesc);
    m_FrameGraph->CreateNode(depthPrePassDesc);
    m_FrameGraph->CreateNode(offscreenPassDesc);
    m_FrameGraph->Compile();

    m_FrameGraph->BindRenderPass(lnnew lne::DepthPrePass());
    m_FrameGraph->BindRenderPass(lnnew lne::BasicForwardPass());
    m_FrameGraph->BindRenderPass(lnnew FinalPass());
}

void AppLayer::OnDetach()
{
    APP_INFO("AppLayer::OnDetach");
    m_BasePipeline.Reset();
}

void AppLayer::OnUpdate(float deltaTime)
{
    m_Scene->BeginScene();
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

    m_WorldRenderer->BeginFrame();
    m_WorldRenderer->Render(*m_Scene.GetPtr());
    m_WorldRenderer->EndFrame();
    
    m_Scene->EndScene();
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
        m_Scene->DestroyEntity(m_DuckEntity);
        m_DuckEntity = lne::Entity{};
    }
    auto& transform = m_CubeEntity.GetComponent<lne::TransformComponent>();
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
