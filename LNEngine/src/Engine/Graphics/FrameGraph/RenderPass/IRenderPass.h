#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/ECS/Types.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/Mesh.h"

namespace lne
{
class FrameGraph;
struct FrameGraphNode;

class IRenderPass : public RefCountBase
{
public:
    IRenderPass() = default;
    virtual ~IRenderPass() = default;

    virtual void OnBind() {}
    virtual void BeginFrame() {}

    virtual void PreExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) {}
    virtual void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) = 0;
    virtual void PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) {}

    virtual void EndFrame() {}
    virtual void Cleanup() {}
    
    virtual void OnResize(glm::vec2 dimension) {}

    std::string_view GetName() const { return m_Name; }
    const EntitySignature& MustHaveComponents() const { return m_MustHaveComponents; }
    virtual std::string_view GetDebugName() const override { return m_Name; }

protected:
    std::string     m_Name{};
    EntitySignature m_MustHaveComponents{};
};

class IDrawStaticMeshes
{
public:
    struct DrawCommand
    {
        SafePtr<class StaticMesh>   Mesh;
        uint32_t                    SubMeshIndex;
        uint32_t                    InstanceCount;
    };
public:
    virtual ~IDrawStaticMeshes() = default;
    virtual void AddStaticMeshDrawCommand(StaticMeshHash hash, SafePtr<class StaticMesh> mesh, uint32_t subMeshIndex);
    void ClearDrawCommands() { m_DrawCommands.clear(); }
protected:
    std::unordered_map<StaticMeshHash, DrawCommand> m_DrawCommands{};
};
}

