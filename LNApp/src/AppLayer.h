#pragma once
#include <LNEInclude.h>

class AppLayer final : public lne::Layer
{
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

    class SkyboxPass : public lne::IRenderPass
    {
    public:
        SkyboxPass()
        {
            m_Name = "SkyboxPass";
        }

        void OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;
        void Execute(vk::CommandBuffer cmdBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;    
        void PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;
        void OnImGuiRender() override;
    private:
        lne::SafePtr<lne::MaterialV2> m_MaterialV2{};
        lne::SafePtr<lne::MaterialV2> m_MaterialV2Test{};
        lne::SafePtr<lne::Texture> m_Texture{};

        lne::SafePtr<lne::Texture> m_DebugTexture{};
        bool m_IsDebugOpen{ false };
    };

    class ToneMappingPass : public lne::IRenderPass
    {
    public:
        ToneMappingPass()
        {
            m_Name = "ToneMappingPass";
        }
        void OnBind(lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;
        void Execute(vk::CommandBuffer cmdBuffer, class lne::WorldRenderer* worldRenderer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;
        void PostExecute(vk::CommandBuffer cmdBuffer, lne::FrameGraph* frameGraph, lne::FrameGraphNode* node) override;
        void OnImGuiRender() override;
    private:
        lne::SafePtr<lne::MaterialV2> m_MaterialV2{};
        lne::SafePtr<lne::Texture> m_DebugTexture{};
        bool m_IsDebugOpen{ false };
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
    lne::SafePtr<lne::GfxPipeline> m_TransparentPipeline{};
    lne::SafePtr<lne::Material> m_BasicMaterial{};
    lne::SafePtr<lne::Material> m_BasicMaterial2{};

    struct CameraTarget
    {
        glm::vec3 Position;
        glm::vec3 Rotation;
    } m_CameraTarget{};

    lne::SafePtr<lne::HierarchicalScene> m_Scene{};
    lne::Entity m_CameraEntity;
    lne::Entity m_ModelEntity;
    lne::Entity m_CubeEntity;
    lne::Entity m_SphereEntity;

    glm::vec3 m_LightDirection{ 1.0f, -1.0f, -1.0f };
    float m_AmbientLight{ 0.03f };
    float m_Metalness{ 0.0f };
    float m_Roughness{ 0.0f };

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
