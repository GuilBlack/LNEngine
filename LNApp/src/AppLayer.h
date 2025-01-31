#pragma once
#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
    // TODO: we really need a scene structure...
    class DepthPrePass : public lne::IRenderPass
    {
    public:
        DepthPrePass()
        {
            m_Name = "DepthPrePass";
        };

        virtual void Render(vk::CommandBuffer, lne::FrameGraphNode* node) override;
    };

    class GBufferPass : public lne::IRenderPass
    {
    public:
        GBufferPass()
        {
            m_Name = "GBufferPass";
        };

        virtual void Render(vk::CommandBuffer, lne::FrameGraphNode* node) override;
    };

    class LightingPass : public lne::IRenderPass
    {
    public:
        LightingPass()
        {
            m_Name = "LightingPass";
        };

        virtual void Render(vk::CommandBuffer, lne::FrameGraphNode* node) override;
    };

public:
    AppLayer()
        : Layer("AppLayer")
    {}
    void OnAttach() override;

    void InitFrameGraph();

    void OnDetach() override;

    void OnUpdate(float deltaTime) override;

    void OnImGuiRender() override;

    bool OnWindowResize(lne::WindowResizeEvent& event);

    void HandleInput(float deltaTime);

private:
    lne::SafePtr<lne::GfxPipeline> m_BasePipeline{};
    lne::SafePtr<lne::Material> m_BasicMaterial{};
    lne::SafePtr<lne::Material> m_BasicMaterial2{};

    lne::SafePtr<lne::GfxPipeline> m_SkyboxPipeline{};
    lne::SafePtr<lne::Material> m_SkyboxMaterial{};

    lne::Geometry m_TesselatedCubeGeo{};
    lne::Geometry m_SphereGeo{};
    lne::SafePtr<lne::Texture> m_Texture{};
    lne::SafePtr<lne::Texture> m_CubemapTexture{};
    lne::SafePtr<lne::StaticMesh> m_Duck{};

    lne::TransformComponent m_DuckTransform{};
    lne::TransformComponent m_CubeTransform{};
    lne::TransformComponent m_SphereTransform{};
    lne::TransformComponent m_SkyboxTransform{};

    struct CameraTarget
    {
        glm::vec3 Position;
        glm::vec3 Rotation;
    } m_CameraTarget{};

    lne::HierarchicalScene m_Scene{};
    lne::Entity m_CameraEntity;

    glm::vec3 m_LightDirection{ 1.0f, -1.0f, -1.0f };
    float m_Metalness{ 0.0f };
    float m_Roughness{ 0.0f };

    lne::FrameGraph m_FrameGraphTest{};
    lne::SafePtr<DepthPrePass> m_DepthPrePass{};
    lne::SafePtr<GBufferPass> m_GBufferPass{};
    lne::SafePtr<LightingPass> m_LightingPass{};

private:
    void GenerateCube(std::vector<lne::Vertex>& vertices, std::vector<uint32_t>& indices, uint32_t tesselationLevel);

    void GenerateUVSphere(std::vector<lne::Vertex>& vertices, std::vector<uint32_t>& indices, float radius = 1.f, uint32_t nLatitude = 32, uint32_t nLongitude = 32);

    float Lerp(float a, float b, float t, float deltaTime)
    {
        return glm::mix(a, b, 1.0f - std::pow(1.0f - t, deltaTime));
    }

    glm::vec3 Lerp3(const glm::vec3& from, const glm::vec3& to, float t, float deltaTime)
    {
        return glm::vec3(
            Lerp(from.x, to.x, t, deltaTime),
            Lerp(from.y, to.y, t, deltaTime),
            Lerp(from.z, to.z, t, deltaTime)
        );
    }
};
