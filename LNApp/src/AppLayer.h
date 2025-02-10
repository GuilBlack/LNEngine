#pragma once
#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{

    class GBufferPass : public lne::IRenderPass
    {
    public:
        GBufferPass()
        {
            m_Name = "GBufferPass";
        }

        virtual void Execute(vk::CommandBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override{}
    };

    class LightingPass : public lne::IRenderPass
    {
    public:
        LightingPass()
        {
            m_Name = "LightingPass";
        }

        virtual void Execute(vk::CommandBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override{}
    };

    class DoFPass : public lne::IRenderPass
    {
    public:
        DoFPass()
        {
            m_Name = "DoFPass";
        }

        virtual void Execute(vk::CommandBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override{}
    };

    class TransparentPass : public lne::IRenderPass
    {
    public:
        TransparentPass()
        {
            m_Name = "TransparentPass";
        }

        virtual void Execute(vk::CommandBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override{}
    };

    class FinalPass : public lne::IRenderPass
    {
    public:
        FinalPass();

        virtual void Execute(vk::CommandBuffer cmdBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;

    private:
        lne::SafePtr<lne::ComputePipeline> m_Pipeline{};
        lne::SafePtr<lne::ComputeProgram> m_Program{};
        lne::SafePtr<lne::Texture> m_OutputTexture{};
    };

public:
    AppLayer()
        : Layer("AppLayer")
    {}
    void OnAttach() override;

    void InitTestFrameGraph();

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

    struct CameraTarget
    {
        glm::vec3 Position;
        glm::vec3 Rotation;
    } m_CameraTarget{};

    lne::SafePtr<lne::HierarchicalScene> m_Scene{};
    lne::Entity m_CameraEntity;
    lne::Entity m_DuckEntity;
    lne::Entity m_CubeEntity;
    lne::Entity m_SphereEntity;

    glm::vec3 m_LightDirection{ 1.0f, -1.0f, -1.0f };
    float m_Metalness{ 0.0f };
    float m_Roughness{ 0.0f };

    lne::FrameGraph m_FrameGraphTest{ "ComplexFrameGraph" };
    lne::SafePtr<lne::FrameGraph> m_FrameGraph{};
    lne::SafePtr<lne::WorldRenderer> m_WorldRenderer{};

private:
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
