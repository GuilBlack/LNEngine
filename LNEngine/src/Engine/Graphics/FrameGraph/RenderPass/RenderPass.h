#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/ECS/Types.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/StructsHashes.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace lne
{
#define LNE_PROFILING_RP_COL 0xE2892F
class FrameGraph;
struct FrameGraphNode;

PassID MakePassID(std::string_view name);

class RenderPass : public RefCountBase
{
public:
    RenderPass() = default;
    virtual ~RenderPass() = default;

    void OnBindInternal(FrameGraph* frameGraph, FrameGraphNode* node);
    virtual void OnBind(FrameGraph* frameGraph, FrameGraphNode* node) {}

    /**
     * BeginFrame is called on the main thread at the start of each frame before any render tasks are submitted.
     * This can be used to reset per-frame data or prepare resources needed for rendering.
     */
    virtual void BeginFrame() {}

    /**
     * PreExecute is called before the Execute method on the render thread.
     * @param cmdBuffer The command buffer to record commands into. this cb can be a primary or secondary command buffer.
     * @param frameGraph The frame graph instance containing this node.
     * @param node The frame graph node associated with this render pass. Can be used to access input/output resources and other node-specific data.
     */
    virtual void PreExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) {}

    /**
     * Execute is called to perform the rendering operations of this pass on the render thread.
     * @param cmdBuffer The command buffer to record commands into. this cb can be a primary or secondary command buffer.
     * @param worldRenderer The world renderer instance, which may provide access to scene data and rendering utilities.
     * @param frameGraph The frame graph instance containing this node.
     * @param node The frame graph node associated with this render pass. Can be used to access input/output resources and other node-specific data.
     */
    virtual void Execute(vk::CommandBuffer cmdBuffer, class WorldRenderer* worldRenderer, FrameGraph* frameGraph, FrameGraphNode* node) = 0;

    /**
     * PostExecute is called after the Execute method on the render thread.
     * @param cmdBuffer The command buffer to record commands into. this cb can be a primary or secondary command buffer.
     * @param frameGraph The frame graph instance containing this node.
     * @param node The frame graph node associated with this render pass. Can be used to access input/output resources and other node-specific data.
     */
    virtual void PostExecute(vk::CommandBuffer cmdBuffer, FrameGraph* frameGraph, FrameGraphNode* node) {}

    /**
     * This is called on the main thread after the render task has been submitted but before the frame is presented.
     */
    virtual void EndFrame() {}
    
    virtual void OnResize(FrameGraph* frameGraph, FrameGraphNode* node) {}

    /**
     * This is called on the main thread to render any ImGui widgets for this render pass.
     */
    virtual void OnImGuiRender() {}

    std::string_view            GetName() const { return m_Name; }
    PassID                      GetID() const { return m_ID; }
    const EntitySignature&      MustHaveComponents() const { return m_MustHaveComponents; }
    virtual std::string_view    GetDebugName() const override { return m_Name; }

protected:
    std::string     m_Name{};
    EntitySignature m_MustHaveComponents{};

private:
    PassID          m_ID{};
};

class IDrawStaticMeshes
{
public:
    struct DrawCommand
    {
        SafePtr<class StaticMesh>   Mesh;
        u32                    SubMeshIndex;
        u32                    InstanceCount;
    };
public:
    IDrawStaticMeshes();
    virtual ~IDrawStaticMeshes() = default;
    virtual void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex) = 0;
    void ClearDrawCommands();
protected:
    std::vector<FlatHashMap<StaticMeshHash, DrawCommand>> m_DrawCommands;
};
}

