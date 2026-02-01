#include "pch.h"
#include "AppLayer.h"

AppLayer::FinalPass::FinalPass()
{
    using namespace lne;
    m_Name = "FinalPass";
    auto context = ApplicationBase::GetRenderer().GetGfxContext();
    m_OutputTexture = lne::Texture::CreateColorAttachmentTexture(context, 1920, 1080, vk::Format::eR8G8B8A8Unorm, TextureUsageType::eSampledAndStorage, "Test");
}

void AppLayer::FinalPass::Execute(vk::CommandBuffer cmdBuffer, lne::WorldRenderer* worldRenderer, 
    lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    m_OutputTexture->TransitionLayout(cmdBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    lne::Framebuffer& swapchainFramebuffer = lne::ApplicationBase::GetWindow().GetFramebuffer(renderer.GetCurrentSwapchainImageIndex());
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

    SafePtr skyboxEffect = renderer.CreateOrGetEffect(lne::ApplicationBase::GetAssetsPath() 
                                                      + "Engine\\Shaders\\Skybox.glsl");

    GfxTechniqueDesc techDesc{};
    techDesc.Name = "SkyboxTechnique";
    techDesc.TechniqueState.Cull = lne::ECullMode::None;
    techDesc.TechniqueState.Fill = lne::EFillMode::Solid;
    techDesc.TechniqueState.Transparency = lne::TransparencyMode::eOpaque;
    techDesc.TechniqueState.DepthMode = lne::DepthMode::eNone;
    techDesc.TechniqueState.DepthCompareOp = lne::ECompareOperation::GreaterOrEqual;

    PassBindingDesc passDesc{};
    passDesc.PassName = "SkyboxPass";
    passDesc.PassEffect = skyboxEffect;
    techDesc.Passes.push_back(passDesc);
    SafePtr technique = renderer.CreateOrGetTechnique(techDesc);

    m_Material = lnnew Material(technique);

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
    LNE_PROFILE_FUNCTION_C(LNE_PROFILING_RP_COL);
    if (m_Texture != worldRenderer->GetEnvironment()->SkyboxTexture)
    {
        m_Texture = worldRenderer->GetEnvironment()->SkyboxTexture;
        m_Material->SetTexture("tCubeAlbedo", m_Texture);
    }

    lne::Renderer& renderer = lne::ApplicationBase::GetRenderer();
    auto lightBuffer = worldRenderer->GetLightBufferGPU(renderer.GetCurrentFrameIndex());
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material, lightBuffer, GetID());
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

    SafePtr toneMapperEffect = renderer.CreateOrGetEffect(ApplicationBase::GetAssetsPath()
                                                          + "Shaders\\ToneMapper.glsl");

    GfxTechniqueDesc techDesc{};
    techDesc.Name = "ToneMapperTechnique";
    techDesc.TechniqueState.Cull = lne::ECullMode::None;
    techDesc.TechniqueState.Fill = lne::EFillMode::Solid;
    techDesc.TechniqueState.Transparency = lne::TransparencyMode::eOpaque;
    techDesc.TechniqueState.DepthMode = lne::DepthMode::eNone;

    PassBindingDesc passDesc{};
    passDesc.PassName = "ToneMappingPass";
    passDesc.PassEffect = toneMapperEffect;
    techDesc.Passes.push_back(passDesc);
    SafePtr technique = renderer.CreateOrGetTechnique(techDesc);

    m_Material = lnnew Material(technique);

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

    for (lne::FrameGraphResourceHandle handle : node->InputResources)
    {
        lne::FrameGraphResource* resource = frameGraph->GetResource(handle);
        lne::SafePtr<lne::Texture> sceneTexture = resource->Resource.GetAs<lne::Texture>();
        m_Material->SetTexture("tSceneTexture", sceneTexture);
    }
}

void AppLayer::ToneMappingPass::OnResize(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node)
{
    for (lne::FrameGraphResourceHandle handle : node->InputResources)
    {
        lne::FrameGraphResource* resource = frameGraph->GetResource(handle);
        lne::SafePtr<lne::Texture> sceneTexture = resource->Resource.GetAs<lne::Texture>();
        m_Material->SetTexture("tSceneTexture", sceneTexture);
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
    }
    auto lightBuffer = worldRenderer->GetLightBufferGPU(renderer.GetCurrentFrameIndex());
    renderer.DrawFullscreenQuad(cmdBuffer, m_Material, lightBuffer, GetID());
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

    SafePtr uvChecker = renderer.CreateTexture(lne::ApplicationBase::GetAssetsPath() + "Textures\\UVChecker.png");

    SafePtr opaqueTechnique = renderer.GetTechnique("DefaultMeshOpaque");

    m_BasicMaterial = lnnew Material(opaqueTechnique);
    m_BasicMaterial2 = lnnew Material(opaqueTechnique);

    m_BasicMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    // m_BasicMaterial->SetTexture("tAlbedo", uvChecker);
    m_BasicMaterial->SetProperty("uMetalness", 1.0f);
    m_BasicMaterial->SetProperty("uRoughness", 0.05f);
    m_BasicMaterial->SetProperty("uUseSSR", 1.0f);

    m_BasicMaterial2->SetProperty("uColor", glm::vec4(1.f, 1.f, 1.f, 1.f));
    m_BasicMaterial2->SetProperty("uMetalness", 0.0f);
    m_BasicMaterial2->SetProperty("uRoughness", 1.0f);

#pragma region CreateEntities
    m_CameraEntity = m_Scene->CreateEntity();

    m_ModelEntity = m_Scene->CreateEntity();
    m_ModelSpheres = m_Scene->CreateEntity();
    m_CubeEntity = m_Scene->CreateEntity();
    m_SphereEntity = m_Scene->CreateEntity();

    m_ModelEntity.EmplaceComponent<StaticMeshComponent>();
    m_ModelSpheres.EmplaceComponent<StaticMeshComponent>();
    m_CubeEntity.EmplaceComponent<StaticMeshComponent>();
    m_SphereEntity.EmplaceComponent<StaticMeshComponent>();

    m_LightEntities.reserve(1);
    for (uint32_t i = 0; i < 10; ++i)
    {
        lne::Entity lightEntity = m_Scene->CreateEntity();
        auto& lightComp = lightEntity.EmplaceComponent<lne::LightComponent>();
        lightComp.Type = lne::LightType::ePoint;
        lightComp.Color = glm::vec3(static_cast<float>(std::rand()) / RAND_MAX,
                                    static_cast<float>(std::rand()) / RAND_MAX,
                                    static_cast<float>(std::rand()) / RAND_MAX);
        float angle = i * glm::two_pi<float>() / 10;
        float radius = 1.f;
        auto& lightTransform = lightEntity.GetComponent<lne::TransformComponent>();
        lightTransform.Position = glm::vec3(cos(angle) * radius, 0.5f, sin(angle) * radius);

        lightComp.Intensity = 100.f;
        lightComp.Range = 1.0f;

        m_LightEntities.emplace_back(std::move(lightEntity));
    }
    lne::Entity spotLightEntity = m_Scene->CreateEntity();
    auto& spotLightComp = spotLightEntity.EmplaceComponent<lne::LightComponent>();
    spotLightComp.Type = lne::LightType::eSpot;
    spotLightComp.Color = glm::vec3(1.0f, 1.0f, 1.0f);
    spotLightComp.Intensity = 100.f;
    spotLightComp.Range = 30.0f;
    spotLightComp.SpotAngle = glm::radians(10.0f);
    auto& spotLightTransform = spotLightEntity.GetComponent<lne::TransformComponent>();
    spotLightTransform.Position = glm::vec3(0.0f, .5f, 0.0f);
    spotLightTransform.SetEulerAngles({ 0.0f, -90.0f, 0.0f });

    CameraComponent& cameraComponent = m_CameraEntity.EmplaceComponent<CameraComponent>();
    TransformComponent& cameraTransform = m_CameraEntity.GetComponent<TransformComponent>();

    auto [modelTransform, modelMeshComponent] = m_ModelEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto [modelSphereTransform, modelSphereMeshComponent] = m_ModelSpheres.GetComponents<TransformComponent, StaticMeshComponent>();
    auto [cubeTransform, cubeMeshComponent] = m_CubeEntity.GetComponents<TransformComponent, StaticMeshComponent>();
    auto [sphereTransform, sphereMeshComponent] = m_SphereEntity.GetComponents<TransformComponent, StaticMeshComponent>();

#pragma endregion

#pragma region LoadModels
    SafePtr cubeMesh = StaticMesh::GenerateCube(1);
    cubeMesh->SetMaterial(m_BasicMaterial, 0);
    SafePtr sphereMesh = StaticMesh::GenerateUVSphere(1.0f, 32, 32);
    sphereMesh->SetMaterial(m_BasicMaterial2, 0);
    modelMeshComponent.Mesh = lnnew StaticMesh(ApplicationBase::GetAssetsPath() + "Models\\gltf\\Models\\Sponza\\glTF\\Sponza.gltf", GeometryType::eMeshlet);

    cubeMeshComponent.Mesh = cubeMesh;
    sphereMeshComponent.Mesh = sphereMesh;
#pragma endregion

#pragma region TransformInit
    cubeTransform.Position =  { 8.0f, -.1f, 0.0f };
    cubeTransform.Scale =     { 2.f, 0.1f, 2.f };

    sphereTransform.Position = { 0.f, .25f, 0.0f };
    sphereTransform.Scale = { .5f, .5f, .5f };

    modelTransform.Position = { 0.0f, 0.0f, 0.0f };
    modelTransform.Scale = { 100.f, 100.f, 100.f };

    modelSphereTransform.Position = { 0.0f, 10.0f, 0.0f };
    modelSphereTransform.SetEulerAngles({ 90.0f, 0.0f, 0.0f });
    modelSphereTransform.Scale = { 10.f, 10.f, 10.f };

#pragma endregion

    cameraTransform.Position = { -6.0f, 1.0f, 0.0f };
    cameraTransform.SetEulerAngles({ 0.f, -90.f, 0.f });

    auto& windowSettings = ApplicationBase::GetWindow().GetSettings();
    cameraComponent.SetPerspective(45.0f, windowSettings.Width / (float)windowSettings.Height, 0.01f, 1000.0f);

    m_CameraTarget.Position = cameraTransform.Position;
    m_CameraTarget.Rotation = cameraTransform.EulerAngles;
    cameraComponent.UpdateView(cameraTransform);

    m_WorldRenderer->SetEnvironmentMap(ApplicationBase::GetAssetsPath() + "Textures\\HDRIs\\pisa.hdr");
    m_WorldRenderer->SetSunLightDirection(m_LightDirection);
    m_WorldRenderer->SetAmbientLight(m_AmbientLight);

    SafePtr meshletSphereMesh = StaticMesh::GenerateUVSphereMeshlets(3.0f, 128, 128);

    SafePtr meshletTech = renderer.GetTechnique("DefaultMeshletOpaque");
    if (meshletTech)
    {
        PipelineHandle pipeline = meshletTech->CreateOrGetPipeline(lne::MakePassID("GBufferPass"), m_FrameGraph);
    }
    m_MeshletMaterial = lnnew Material(meshletTech);
    m_MeshletMaterial->SetProperty("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    meshletSphereMesh->SetMaterial(m_MeshletMaterial, 0);
    lne::Entity meshletSphereEntity = m_Scene->CreateEntity();
    meshletSphereEntity.EmplaceComponent<lne::StaticMeshComponent>();

    auto [meshletTransform, meshletMeshComponent] = meshletSphereEntity.GetComponents<lne::TransformComponent, lne::StaticMeshComponent>();

    meshletMeshComponent.Mesh = meshletSphereMesh;
    meshletTransform.Position = { 0.0f, 15.0f, 0.0f };

    glm::vec3 color{};

    for (int sic = 0; sic < 2; ++sic)
    {
        if (sic == 0)
            color = 0.8f * glm::vec3(1.0f, 1.0f, 1.0f);
        else
            color = glm::vec3(0.8, 0.6941176470588235f, 0.11372549019607843f); // gold-like

        for (int sim = 0; sim < 5; ++sim)
        {
            float metalness = sim / 4.0f;
            for (int sir = 0; sir < 5; ++sir)
            {
                float roughness = sir / 4.0f;
                lne::Entity entity = m_Scene->CreateEntity();
                entity.EmplaceComponent<lne::StaticMeshComponent>();

                SafePtr metalRoughSphereMesh = meshletSphereMesh->Clone();
                SafePtr grayMaterial = lnnew Material(meshletTech);
                grayMaterial->SetProperty("uColor", glm::vec4(color, 1.0f));
                grayMaterial->SetProperty("uMetalness", metalness);
                grayMaterial->SetProperty("uRoughness", roughness);
                grayMaterial->SetProperty("uUseSSR", 1.0f);
                metalRoughSphereMesh->SetMaterial(grayMaterial, 0);
                auto [transform, meshComponent] = entity.GetComponents<lne::TransformComponent, lne::StaticMeshComponent>();
                meshComponent.Mesh = metalRoughSphereMesh;

                transform.Position = glm::vec3(
                    (sim * 2.0f - 1.0f) * 5.0f,
                    (sir * 2.0f - 1.0f) * 5.0f,
                    10.0f * (sic + 1)
                );
            }
        }
    }
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

    lne::FrameGraphResourceDesc metalRoughAttachmentDesc = resourceBuilder.SetName("GBufferMetalRough")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR8G8Unorm)
        .Build();

    lne::FrameGraphResourceDesc lightingResource = resourceBuilder.SetName("Lighting")
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultColorAttachmentInfos()
        .SetImageFormat(vk::Format::eR16G16B16A16Sfloat)
        .SetImageUseMips(true)
        .Build();
    resourceBuilder.SetImageUseMips(false);

    lne::FrameGraphResourceDescBuilder mainScenePyramidResourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphResourceDesc lightingResourceProxy = mainScenePyramidResourceBuilder.SetName("LightingPyramidRef")
        .SetImageDimension(width, height)
        .SetType(lne::FrameGraphResourceType::eProxy).SetProxyInfo("Lighting")
        .Build();

    lne::FrameGraphResourceDesc skyboxResource = resourceBuilder.SetName("LightingSkyboxRef")
        .SetType(lne::FrameGraphResourceType::eProxy).SetProxyInfo("LightingTransparentRef")
        .Build();

    lne::FrameGraphResourceDescBuilder transparentResourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphResourceDesc transparentResource = transparentResourceBuilder.SetName("LightingTransparentRef")
        .SetImageDimension(width, height)
        .SetType(lne::FrameGraphResourceType::eProxy).SetProxyInfo("MeshletDebugRef")
        .Build();

    lne::FrameGraphResourceDescBuilder meshletDebugResourceBuilder = lne::FrameGraphResourceDescBuilder();
    lne::FrameGraphResourceDesc meshletDebugResource = meshletDebugResourceBuilder.SetName("MeshletDebugRef")
        .SetImageDimension(width, height)
        .SetType(lne::FrameGraphResourceType::eProxy).SetProxyInfo("SsrScene")
        .Build();

    lne::FrameGraphResourceDesc depthAttachmentDesc = resourceBuilder
        .SetType(lne::FrameGraphResourceType::eAttachment)
        .SetDefaultDepthAttachmentInfos().SetName("Depth").Build();

    lne::FrameGraphResourceDesc toneMappedSceneDesc = resourceBuilder
        .SetDefaultColorAttachmentInfos().SetName("ToneMappedScene")
        .Build();

    lne::FrameGraphResourceDesc ssrSceneDesc = resourceBuilder
        .SetDefaultColorAttachmentInfos().SetImageFormat(vk::Format::eR16G16B16A16Sfloat).SetName("SsrScene")
        .Build();

    lne::FrameGraphResourceDesc depthTextureDesc = resourceBuilder
        .SetType(lne::FrameGraphResourceType::eTexture)
        .SetDefaultDepthAttachmentInfos()
        .SetName("Depth").Build();

    lne::FrameGraphNodeDesc depthPrePassDesc = nodeBuilder.SetName("DepthPrePass")
        .AddOutputResource(depthAttachmentDesc)
        .Build();

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc finalPassDesc = nodeBuilder.SetName("FinalPass")
        .AddInputResource(toneMappedSceneDesc)
        .SetType(lne::RenderPassType::eTransfer)
        .Build();

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc meshletDebugPassDesc = nodeBuilder.SetName("MeshletDebugPass")
        .AddInputResource(ssrSceneDesc)
        .AddInputResource(depthAttachmentDesc)
        .AddOutputResource(meshletDebugResource)
        .SetEnabled(false)
        .Build();

    meshletDebugResource.Type = lne::FrameGraphResourceType::eAttachment;
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc transparentPassDesc = nodeBuilder.SetName("TransparentForwardPass")
        .AddInputResource(meshletDebugResource)
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
        .AddOutputResource(colorAttachmentDesc).AddOutputResource(normalAttachmentDesc).AddOutputResource(metalRoughAttachmentDesc)
        .Build();

    normalAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    colorAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;
    metalRoughAttachmentDesc.Type = lne::FrameGraphResourceType::eTexture;

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc lightingPassDesc = nodeBuilder.SetName("LightingPass")
        .AddInputResource(normalAttachmentDesc)
        .AddInputResource(colorAttachmentDesc)
        .AddInputResource(metalRoughAttachmentDesc)
        .AddInputResource(depthTextureDesc)
        .AddOutputResource(lightingResource)
        .Build();

    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc mainScenePyramidPass = nodeBuilder.SetName("MainScenePyramidPass")
        .AddInputResource(lightingResource)
        .AddOutputResource(lightingResourceProxy)
        .SetType(lne::RenderPassType::eTransfer)
        .Build();

    lightingResourceProxy.Type = lne::FrameGraphResourceType::eTexture;
    nodeBuilder.Clear();
    lne::FrameGraphNodeDesc ssrPassDesc = nodeBuilder.SetName("SsrPass")
        .AddInputResource(lightingResourceProxy)
        .AddInputResource(normalAttachmentDesc)
        .AddInputResource(colorAttachmentDesc)
        .AddInputResource(metalRoughAttachmentDesc)
        .AddInputResource(depthTextureDesc)
        .AddOutputResource(ssrSceneDesc)
        .Build();

    m_FrameGraph->CreateNodes({
        finalPassDesc, skyboxPassDesc, depthPrePassDesc,
        transparentPassDesc, gBufferPassDesc, lightingPassDesc,
        toneMappingPassDesc, meshletDebugPassDesc, ssrPassDesc, mainScenePyramidPass
    });
    m_FrameGraph->Compile();

    m_FrameGraph->BindRenderPass(lnnew lne::LightingPass());
    m_FrameGraph->BindRenderPass(lnnew lne::DepthPrePass());
    m_FrameGraph->BindRenderPass(lnnew lne::TransparentForwardPass());
    m_FrameGraph->BindRenderPass(lnnew FinalPass());
    m_FrameGraph->BindRenderPass(lnnew SkyboxPass());
    m_FrameGraph->BindRenderPass(lnnew lne::GBufferPass());
    m_FrameGraph->BindRenderPass(lnnew ToneMappingPass());
    m_FrameGraph->BindRenderPass(lnnew lne::MeshletDebugPass());
    m_FrameGraph->BindRenderPass(lnnew lne::SsrPass());
    m_FrameGraph->BindRenderPass(lnnew lne::ScenePyramidPass("MainScenePyramidPass", "Lighting"));
}

void AppLayer::OnDetach()
{
    APP_INFO("AppLayer::OnDetach");
    m_WorldRenderer.Reset();
    m_FrameGraph.Reset();
    m_BasicMaterial.Reset();
    m_BasicMaterial2.Reset();
    m_Scene.Reset();
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

    for (int i = 0; i < m_LightEntities.size(); ++i)
    {
        auto& lightTransform = m_LightEntities[i].GetComponent<lne::TransformComponent>();
        float angle = (float)currentTime * 0.5f + i * glm::two_pi<float>() / m_LightEntities.size();
        float radius = 1.f;
        lightTransform.Position = glm::vec3(cos(angle) * radius, .5f, sin(angle) * radius);
    }

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
        m_Scene->RemoveEntity(m_ModelEntity);
        m_ModelEntity = lne::Entity{};
    }
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
        movementInput -= camTransform.GetUp();
    if (inputManager.IsKeyPressed(lne::eKeyE))
        movementInput += camTransform.GetUp();

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

    const float epsilon = 1e-3f;
    if (glm::distance(camTransform.Position, m_CameraTarget.Position) < epsilon)
        camTransform.Position = m_CameraTarget.Position;
    else
        camTransform.Position = Lerp3(camTransform.Position, m_CameraTarget.Position, positionLerpFactor, deltaTime);

    if (glm::distance(camTransform.EulerAngles, m_CameraTarget.Rotation) < epsilon)
        camTransform.SetEulerAngles(m_CameraTarget.Rotation);
    else
        camTransform.SetEulerAngles(Lerp3(camTransform.EulerAngles, m_CameraTarget.Rotation, rotationLerpFactor, deltaTime));

    cam.UpdateView(camTransform);
}
