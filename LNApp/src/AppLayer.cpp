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
    m_OutputTexture = lne::Texture::CreateColorAttachmentTexture(context, 1920, 1080, vk::Format::eR8G8B8A8Unorm, TextureUsageType::eSampledAndStorage, "Test");

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
    lne::FrameGraphResource* colorResource = frameGraph->GetResource("ToneMappedScene");
    if (colorResource == nullptr)
    {
        APP_WARN("FinalPass::Render: Color resource not found");
        return;
    }
    lne::SafePtr<lne::Texture> colorTexture = colorResource->Resource.GetAs<lne::Texture>();
    renderer.Blit(cmdBuffer, colorTexture, renderTexture);
}

void AppLayer::SkyboxPass::OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    lne::GraphicsPipelineDesc desc{};
    desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\Skybox.glsl";
    desc.Name = "Skybox";
    desc.FrameGraph = frameGraph;
    desc.EnableDepthTest(true, false);
    desc.Blend.EnableBlend(false);
    desc.CullMode = lne::ECullMode::None;
    m_Pipeline = renderer.CreateGraphicsPipeline(desc);
    m_Material = lne::SafePtr(lnnew lne::Material(m_Pipeline, lne::MaterialType::ePostProcess));

    SafePtr gbufferTestEffect = lnnew lne::Effect(lne::ApplicationBase::GetRenderer().GetGfxContext(),
                                                  lne::ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\GBufferEffectTest.glsl");
    lne::GraphicsPipelineDescV2 desc2{};
    desc2.CullMode = lne::ECullMode::Back;
    desc2.Fill = lne::EFillMode::Solid;
    desc2.TransparencyMode = lne::TransparencyMode::eOpaque;
    desc2.FrameGraph = frameGraph;
    auto handle = gbufferTestEffect->CreateOrGetPipeline(desc2);

    for (FrameGraphResourceHandle resourceHandle : node->InputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<lne::Texture> texture = resource.Resource.GetAs<lne::Texture>();
            if (texture->IsDepth())
                continue;
            SafePtr<lne::Texture> debugTexture = lne::Texture::CreateColorTexture2D(ApplicationBase::GetWindow().GetGfxContext(),
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTexture = debugTexture;
        }
    }
}

void AppLayer::SkyboxPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer,
    lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    if (m_Texture != worldRenderer->GetEnvironment()->SkyboxTexture)
    {
        m_Texture = worldRenderer->GetEnvironment()->SkyboxTexture;
        m_Material->SetTexture("tCubeAlbedo", m_Texture);
    }
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material);
}

void AppLayer::SkyboxPass::PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    if (m_IsDebugOpen == false)
        return;

    Renderer& renderer = ApplicationBase::GetRenderer();
    for (FrameGraphResourceHandle resourceHandle : node->InputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);
        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            if (texture->IsDepth())
                continue;
            SafePtr<Texture> debugTexture = m_DebugTexture;
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderer.Blit(cmdBuffer, texture, debugTexture);
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

void AppLayer::SkyboxPass::OnImGuiRender()
{
    float textureRatio = (float)m_DebugTexture->GetDimensions().width / (float)m_DebugTexture->GetDimensions().height;
    float windowWidth = ImGui::GetWindowWidth();
    m_IsDebugOpen = ImGui::TreeNode("Skybox Pass Output");
    if (m_IsDebugOpen)
    {
        ImGui::Image((ImTextureID)(uint64_t)m_DebugTexture->GetBindlessTextureHandle(), ImVec2(windowWidth, windowWidth / textureRatio));
        ImGui::TreePop();
    }
}

void AppLayer::ToneMappingPass::OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    lne::GraphicsPipelineDesc desc{};
    desc.PathToShaders = lne::ApplicationBase::GetAssetsPath() + "Shaders\\ToneMapper.glsl";
    desc.Name = "ToneMapper";
    desc.FrameGraph = frameGraph;
    desc.EnableDepthTest(false, false);
    desc.Blend.EnableBlend(false);
    desc.CullMode = lne::ECullMode::None;
    m_Pipeline = renderer.CreateGraphicsPipeline(desc);
    m_Material = lne::SafePtr(lnnew lne::Material(m_Pipeline, lne::MaterialType::ePostProcess));

    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);

        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            SafePtr<Texture> debugTexture = Texture::CreateColorTexture2D(ApplicationBase::GetWindow().GetGfxContext(),
                texture->GetDimensions().width / 2, texture->GetDimensions().height / 2, texture->GetFormat(), TextureUsageType::eSampled, false, texture->GetName() + "ImGUI Debug");
            m_DebugTexture = debugTexture;
        }
    }
}

void AppLayer::ToneMappingPass::Execute(vk::CommandBuffer cmdBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL)
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    for (lne::FrameGraphResourceHandle handle : node->InputResources)
    {
        lne::FrameGraphResource* resource = frameGraph->GetResource(handle);
        lne::SafePtr<lne::Texture> sceneTexture = resource->Resource.GetAs<lne::Texture>();
        m_Material->SetTexture("tSceneTexture", sceneTexture);
    }
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material);
}

void AppLayer::ToneMappingPass::PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    using namespace lne;
    if (m_IsDebugOpen == false)
        return;

    Renderer& renderer = ApplicationBase::GetRenderer();
    for (FrameGraphResourceHandle resourceHandle : node->OutputResources)
    {
        FrameGraphResource& resource = *frameGraph->GetResource(resourceHandle);
        if (resource.Type == FrameGraphResourceType::eAttachment)
        {
            SafePtr<Texture> texture = resource.Resource.GetAs<Texture>();
            if (texture->IsDepth())
                continue;
            SafePtr<Texture> debugTexture = m_DebugTexture;
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderer.Blit(cmdBuffer, texture, debugTexture);
            debugTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }
}

void AppLayer::ToneMappingPass::OnImGuiRender()
{
    float textureRatio = (float)m_DebugTexture->GetDimensions().width / (float)m_DebugTexture->GetDimensions().height;
    float windowWidth = ImGui::GetWindowWidth();
    m_IsDebugOpen = ImGui::TreeNode("Tone Mapping Pass Output");
    if (m_IsDebugOpen)
    {
        ImGui::Image((ImTextureID)(uint64_t)m_DebugTexture->GetBindlessTextureHandle(), ImVec2(windowWidth, windowWidth / textureRatio));
        ImGui::TreePop();
    }
}

void AppLayer::OnAttach()
{
    using namespace lne;
    APP_INFO("AppLayer::OnAttach");
    ApplicationBase::GetEventHub().RegisterListener<WindowResizeEvent>(this, &AppLayer::OnWindowResize, 10);

    Renderer& renderer = ApplicationBase::GetRenderer();
    m_Scene = lnnew HierarchicalScene();
    InitFrameGraph();
    m_WorldRenderer = lnnew WorldRenderer(m_FrameGraph);

    auto& fb = ApplicationBase::GetWindow().GetCurrentFramebuffer();
    fb.SetClearColor({0.105f, 0.117f, 0.149f, 1.0f });
    GraphicsPipelineDesc desc{};
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\GBuffer.glsl";
    desc.Name = "GBufferBasic";
    desc.FrameGraph = m_FrameGraph.GetPtr();
    desc.CullMode = ECullMode::Back; 
    desc.EnableDepthTest(true, true, lne::ECompareOperation::LessOrEqual);
    desc.Blend.EnableBlend(false);

    m_BasePipeline = renderer.CreateGraphicsPipeline(desc);
    m_BasicMaterial = lnnew Material(m_BasePipeline);
    m_BasicMaterial2 = lnnew Material(m_BasePipeline);

    desc.EnableDepthTest(true, true);
    desc.Blend.EnableBlend(true);
    desc.Blend.SetAlpha(vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd)
        .SetColor(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd);
    desc.Name = "TransparentForward";
    desc.PathToShaders = ApplicationBase::GetAssetsPath() + "Engine\\Shaders\\ForwardTransparent.glsl";
    m_TransparentPipeline = renderer.CreateGraphicsPipeline(desc);

    SafePtr uvChecker = renderer.CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");

    m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_BasicMaterial->SetTexture("tAlbedo", uvChecker);
    m_BasicMaterial2->SetProperty("uColor", glm::vec4(0.25f, 0.25f, 0.25f, 0.25f));

#pragma region CreateEntities
    m_CameraEntity = m_Scene->CreateEntity();
    CameraComponent& cameraComponent = m_CameraEntity.EmplaceComponent<CameraComponent>();
    TransformComponent& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();

    m_ModelEntity = m_Scene->CreateEntity();
    m_CubeEntity = m_Scene->CreateEntity();
    m_SphereEntity = m_Scene->CreateEntity();
    
    m_ModelEntity.EmplaceComponent<StaticMeshComponent>();
    m_CubeEntity.EmplaceComponent<StaticMeshComponent>();
    m_SphereEntity.EmplaceComponent<StaticMeshComponent>();

    auto[modelTransform, modelMeshComponent] = m_ModelEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[cubeTransform, cubeMeshComponent] = m_CubeEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto[sphereTransform, sphereMeshComponent] = m_SphereEntity.GetComponents<TransformComponent, StaticMeshComponent>();
#pragma endregion

#pragma region LoadModels
    SafePtr cubeMesh = StaticMesh::GenerateCube(1);
    cubeMesh->SetMaterial(m_BasicMaterial, 0);
    SafePtr sphereMesh = StaticMesh::GenerateUVSphere(1.0f, 32, 32);
    sphereMesh->SetMaterial(m_BasicMaterial2, 0);
    modelMeshComponent.Mesh = lnnew StaticMesh(ApplicationBase::GetAssetsPath() + "Models\\gltf\\Models\\Sponza\\glTF\\Sponza.gltf", m_BasePipeline, m_TransparentPipeline);

    cubeMeshComponent.Mesh = cubeMesh;
    sphereMeshComponent.Mesh = sphereMesh;
#pragma endregion

#pragma region TransformInit
    cubeTransform.Position =  { -0.5f, 0.0f, -30.0f };
    cubeTransform.Scale =     { 0.25f, 0.25f, 0.25f };

    sphereTransform.Position = { 0.5f, 0.0f, 0.0f };
    sphereTransform.Scale =    { 0.25f, 0.25f, 0.25f };

    modelTransform.Position = { 0.0f, 0.0f, 0.0f };
    modelTransform.Scale = { 100.f, 100.f, 100.f };

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
    //        tempMeshComp.Mesh = sphereMesh;
    //}

    cameraTransform.Position = { -2.0f, 0.0f, 0.0f };
    cameraTransform.LookAt({ 0.0f, 0.0f, 0.0f });

    auto& windowSettings = ApplicationBase::GetWindow().GetSettings();
    cameraComponent.SetPerspective(45.0f, windowSettings.Width / (float)windowSettings.Height, 0.001f, 1000.0f);

    m_CameraTarget.Position = cameraTransform.Position;
    m_CameraTarget.Rotation = cameraTransform.EulerAngles;
    cameraComponent.UpdateView(cameraTransform);

    m_WorldRenderer->SetEnvironmentMap(ApplicationBase::GetAssetsPath() + "Textures\\HDRIs\\OvercastIndustrialCourtyard.hdr");
    m_WorldRenderer->SetSunLightDirection(m_LightDirection);
    m_WorldRenderer->SetAmbientLight(m_AmbientLight);
}

void AppLayer::InitFrameGraph()
{
    auto [width, height] = lne::ApplicationBase::GetWindow().GetSwapchain()->GetViewport().GetExtent();
    m_FrameGraph = lnnew lne::FrameGraph("BasicFrameGraph");
    lne::FrameGraphResourceDescBuilder resourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphNodeDescBuilder nodeBuilder = lne::FrameGraphNodeDescBuilder();

    lne::FrameGraphResourceDesc colorAttachmentDesc = resourceBuilder.SetName("GBufferColor")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageDimension(width, height)
    .Build();
    
    lne::FrameGraphResourceDesc normalAttachmentDesc = resourceBuilder.SetName("GBufferNormal")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .Build();
    
    lne::FrameGraphResourceDesc positionAttachmentDesc = resourceBuilder.SetName("GBufferPosition")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .Build();
    
    lne::FrameGraphResourceDesc metalRoughAttachmentDesc = resourceBuilder.SetName("GBufferMetalRough")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR8G8Unorm)
        .Build();
    
    lne::FrameGraphResourceDesc lightingResource = resourceBuilder.SetName("Lighting")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .Build();

    lne::FrameGraphResourceDesc skyboxResource = resourceBuilder.SetName("LightingSkyboxRef")
        .SetType(lne::FrameGraphResourceType::eProxy)
        .SetProxyInfo("LightingTransparentRef")
        .Build();

    lne::FrameGraphResourceDescBuilder transparentResourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphResourceDesc transparentResource = transparentResourceBuilder.SetName("LightingTransparentRef")
        .SetImageDimension(width, height)
        .SetType(lne::FrameGraphResourceType::eProxy)
        .SetProxyInfo("Lighting")
        .Build();

    lne::FrameGraphResourceDesc depthAttachmentDesc = resourceBuilder
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultDepthAttachmentInfos()
        .SetName("Depth").Build();

    lne::FrameGraphResourceDesc toneMappedSceneDesc = resourceBuilder
        .SetDefaultColorAttachmentInfos()
        .SetName("ToneMappedScene")
        .Build();

    lne::FrameGraphNodeDesc depthPrePassDesc = nodeBuilder.SetName("DepthPrePass")
        .AddOutputResource(depthAttachmentDesc)
        .Build();

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc finalPassDesc = nodeBuilder.SetName("FinalPass")
        .AddInputResource(toneMappedSceneDesc)
        .SetType(lne::RenderPassType::eTransfer)
        .Build();

    auto otherDepth = depthAttachmentDesc;
    otherDepth.Name = "DepthTest";
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc transparentPassDesc = nodeBuilder.SetName("TransparentForwardPass")
        .AddInputResource(lightingResource)
        .AddInputResource(depthAttachmentDesc)
        .AddOutputResource(transparentResource)
        .Build();

    transparentResource.Type = lne::FrameGraphResourceType::eAttachment;
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc skyboxPassDesc = nodeBuilder.SetName("SkyboxPass")
        .AddInputResource(transparentResource)
        .AddInputResource(depthAttachmentDesc)
        .AddOutputResource(skyboxResource)
        .Build();

    nodeBuilder.Clear();
    skyboxResource.Type = lne::FrameGraphResourceType::eTexture;
    lne::FrameGraphNodeDesc toneMappingPassDesc = nodeBuilder.SetName("ToneMappingPass")
        .AddInputResource(skyboxResource)
        .AddOutputResource(toneMappedSceneDesc)
        .SetType(lne::RenderPassType::eGraphics)
        .Build();
    
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc gBufferPassDesc = nodeBuilder.SetName("GBufferPass")
        .AddInputResource(depthAttachmentDesc)
        .AddOutputResource(colorAttachmentDesc)
        .AddOutputResource(normalAttachmentDesc)
        .AddOutputResource(positionAttachmentDesc)
        .AddOutputResource(metalRoughAttachmentDesc)
        .Build();

    normalAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    positionAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    colorAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    metalRoughAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc lightingPassDesc = nodeBuilder.SetName("LightingPass")
        .AddInputResource(normalAttachmentDesc)
        .AddInputResource(positionAttachmentDesc)
        .AddInputResource(colorAttachmentDesc)
        .AddInputResource(metalRoughAttachmentDesc)
        .AddOutputResource(lightingResource)
        .Build();

    m_FrameGraph->CreateNode(finalPassDesc);
    m_FrameGraph->CreateNode(skyboxPassDesc);
    m_FrameGraph->CreateNode(depthPrePassDesc);
    m_FrameGraph->CreateNode(transparentPassDesc);
    m_FrameGraph->CreateNode(gBufferPassDesc);
    m_FrameGraph->CreateNode(lightingPassDesc);
    m_FrameGraph->CreateNode(toneMappingPassDesc);
    m_FrameGraph->Compile();

    m_FrameGraph->BindRenderPass(lnnew lne::LightingPass());
    m_FrameGraph->BindRenderPass(lnnew lne::DepthPrePass());
    m_FrameGraph->BindRenderPass(lnnew lne::TransparentForwardPass());
    m_FrameGraph->BindRenderPass(lnnew FinalPass());
    m_FrameGraph->BindRenderPass(lnnew SkyboxPass());
    m_FrameGraph->BindRenderPass(lnnew lne::GBufferPass());
    m_FrameGraph->BindRenderPass(lnnew ToneMappingPass());
}

void AppLayer::OnDetach()
{
    APP_INFO("AppLayer::OnDetach");
    m_WorldRenderer.Reset();
    m_FrameGraph.Reset();
    m_BasePipeline.Reset();
    m_TransparentPipeline.Reset();
    m_BasicMaterial.Reset();
    m_BasicMaterial2.Reset();
    m_Scene.Reset();
    m_BasePipeline.Reset();
}

void AppLayer::OnUpdate(float deltaTime)
{
    LNE_PROFILE_FUNCTION()
    m_Scene->BeginScene();
    double currentTime = lne::ApplicationBase::GetClock().GetElapsedTime();
    HandleInput(deltaTime);
    static int frameIndex = 0;
    ++frameIndex;

    float sinTime = (float)sin(currentTime);
    float cosTime = (float)cos(currentTime);

    m_CubeEntity.GetComponent<lne::TransformComponent>().Position.y = sinTime * 0.5f;

    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();

    auto& fb = lne::ApplicationBase::GetWindow().GetCurrentFramebuffer();

    m_WorldRenderer->BeginScene(m_CameraEntity);
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
    bool changeX = ImGui::DragFloat("X", &m_LightDirection.x, 0.1f, -FLT_MAX, FLT_MAX, "%.1f");
    ImGui::SameLine();  // This will keep the next input on the same line
    bool changeY = ImGui::DragFloat("Y", &m_LightDirection.y, 0.1f, -FLT_MAX, FLT_MAX, "%.1f");
    ImGui::SameLine();  // This will keep the next input on the same line
    bool changeZ = ImGui::DragFloat("Z", &m_LightDirection.z, 0.1f, -FLT_MAX, FLT_MAX, "%.1f");
    ImGui::PopItemWidth(); // Restore the previous item width
    if (changeX || changeY || changeZ)
        m_WorldRenderer->SetSunLightDirection(m_LightDirection);

    ImGui::DragFloat("Ambient Light", &m_AmbientLight, 0.001f, 0.f, 1.0f, "%.3f");

    if (ImGui::Button("Delete Model"))
    {
        m_Scene->DestroyEntity(m_ModelEntity);
        m_ModelEntity = lne::Entity{};
    }
    auto& transform = m_CubeEntity.GetComponent<lne::TransformComponent>();
    ImGui::Text("This is some useful text.");
    ImGui::End();

    m_FrameGraph->RenderImGui();
}

bool AppLayer::OnWindowResize(lne::WindowResizeEvent& event) 
{
    APP_INFO("WindowResizeEvent received: {0}x{1}", event.GetWidth(), event.GetHeight());
    if (event.GetWidth() == 0 || event.GetHeight() == 0)
        return false;
    m_CameraEntity.GetComponent<lne::CameraComponent>().SetPerspective(45.0f, event.GetWidth() / (float)event.GetHeight(), 0.01f, 1000.0f);
    m_FrameGraph->OnResize(event);
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
