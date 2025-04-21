#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{
class FrameGraph;

class PipelineBase : public RefCountBase
{
public:
    virtual ~PipelineBase();

    // Pure virtual binding method
    void Bind(const vk::CommandBuffer& cmdBuffer) const;

    SafePtr<GfxContext> GetContext() const { return m_Context; }
    vk::PipelineLayout GetLayout() const { return m_Layout; }
    vk::Pipeline GetPipeline() const { return m_Pipeline; }
    [[nodiscard]] std::vector<vk::DescriptorSetLayout> GetDescriptorSetLayouts() const { return m_Shader->GetDescriptorSetLayouts(); }

protected:
    PipelineBase(SafePtr<GfxContext> ctx, const std::string& name, vk::PipelineBindPoint bindPoint);

    vk::PipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts);

    virtual std::string_view GetDebugName() const override
    {
        return m_Name;
    }

protected:
    SafePtr<GfxContext>     m_Context{};
    SafePtr<Shader>         m_Shader{};
    vk::Pipeline            m_Pipeline{};
    vk::PipelineLayout      m_Layout{};
    vk::PipelineBindPoint   m_BindPoint{};
    std::string             m_Name{};
};

#pragma region Graphics pipeline

struct DepthDesc
{
    bool                DepthTestEnable = false;
    bool                DepthWriteEnable = false;
    ECompareOperation       DepthCompareOp = ECompareOperation::Less;
    bool                StencilTestEnable = false;

    DepthDesc& SetDepthTest(bool read, bool write, ECompareOperation compare);
};

struct BlendState
{
    vk::BlendFactor     SrcColor = vk::BlendFactor::eOne;
    vk::BlendFactor     DstColor = vk::BlendFactor::eOne;
    vk::BlendOp         ColorOp = vk::BlendOp::eAdd;

    vk::BlendFactor     SrcAlpha = vk::BlendFactor::eOne;
    vk::BlendFactor     DstAlpha = vk::BlendFactor::eOne;
    vk::BlendOp         AlphaOp = vk::BlendOp::eAdd;

    bool                BlendEnable = false;
    bool                SepareteAlphaBlendEnable = false;

    EBlendColorWriteMask ColorWriteMask = EBlendColorWriteMask::All;

    BlendState& EnableBlend(bool enable) { BlendEnable = enable; return *this; }
    BlendState& SetColor(vk::BlendFactor srcColor, vk::BlendFactor dstColor, vk::BlendOp colorOp);
    BlendState& SetAlpha(vk::BlendFactor srcAlpha, vk::BlendFactor dstAlpha, vk::BlendOp alphaOp);
    BlendState& SetColorWriteMask(EBlendColorWriteMask mask);
};

struct GraphicsPipelineDesc
{
    std::string                         Name{};
    std::string                         PathToShaders{};
    std::unordered_set<ShaderStage::Enum>    ShaderStages{};

    // rasterization settings
    ECullMode   CullMode = ECullMode::Back;
    EWindingOrder WindingOrder = EWindingOrder::CounterClockwise;
    EFillMode Fill = EFillMode::Solid;

    // depth stencil settings
    DepthDesc   Depth{};
    BlendState  Blend{};

    FrameGraph* FrameGraph = nullptr;

    GraphicsPipelineDesc& SetName(const std::string& name) { Name = name; return *this; }
    GraphicsPipelineDesc& AddStage(ShaderStage::Enum stage) { ShaderStages.insert(stage); return *this; }
    GraphicsPipelineDesc& SetCulling(ECullMode cullMode) { CullMode = cullMode; return *this; }
    GraphicsPipelineDesc& SetWinding(EWindingOrder front) { WindingOrder = front; return *this; }
    GraphicsPipelineDesc& SetFill(EFillMode fill) { Fill = fill; return *this; }
    GraphicsPipelineDesc& EnableDepthTest(bool enable, bool write, ECompareOperation compareOp = ECompareOperation::LessOrEqual)
    {
        Depth.SetDepthTest(enable, write, compareOp); return *this;
    }
};

class GfxPipeline : public PipelineBase
{
public:
    GfxPipeline(SafePtr<class GfxContext> ctx, const GraphicsPipelineDesc& desc);
    virtual ~GfxPipeline() {}

private:
    GraphicsPipelineDesc        m_Desc{};
    std::string                 m_AssociatedRenderPassName{};
    uint64_t                    m_AssociatedRenderPassNameHash{};

    friend class Material;
};

#pragma endregion

#pragma region Compute pipeline

struct ComputePipelineDesc
{
    std::string                         Name{};
    std::string                         PathToShader{};
};

class ComputePipeline : public PipelineBase
{
public:
    ComputePipeline(SafePtr<GfxContext> ctx, const ComputePipelineDesc& desc);
    virtual ~ComputePipeline() override {}

private:
    ComputePipelineDesc m_Desc;

    friend class ComputeProgram;
};
#pragma endregion

}
