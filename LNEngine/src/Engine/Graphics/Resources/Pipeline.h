#pragma once
#include "Shader.h"
#include "Engine/Graphics/Framebuffer.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/GlobalUtils.h"

namespace lne
{
class FrameGraph;
class GfxContext;
class Shader;
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

    vk::PipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts,
                                            const std::vector<vk::PushConstantRange>& pcRanges);

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

struct DepthState
{
    bool                DepthTestEnable = false;
    bool                DepthWriteEnable = false;
    ECompareOperation       DepthCompareOp = ECompareOperation::Less;
    bool                StencilTestEnable = false;

    DepthState& SetDepthTest(bool read, bool write, ECompareOperation compare);
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
    DepthState   Depth{};
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

struct GraphicsPipelineDescV2
{
    FrameGraph*             FrameGraph = nullptr;

    ECullMode               CullMode = ECullMode::Back;
    EFillMode               Fill = EFillMode::Solid;

    TransparencyMode::Enum  TransparencyMode = TransparencyMode::eOpaque;

    bool                    DeriveDepthFromTransparency = true; // if true, Depth and Blend settings will be overridden based on TransparencyMode
    DepthMode::Enum         DepthMode = DepthMode::eReadWrite;
    ECompareOperation       DepthCompareOp = ECompareOperation::LessOrEqual;
private:
    friend class Effect;
    friend class GfxPipeline;
    SafePtr<Shader>         Shader{};
};

inline BlendState MakeBlendState(TransparencyMode::Enum mode,
                                 EBlendColorWriteMask mask = EBlendColorWriteMask::All)
{
    BlendState s{};
    s.ColorWriteMask = mask;

    auto set = [&](vk::BlendFactor sc, vk::BlendFactor dc, vk::BlendOp co,
                   vk::BlendFactor sa, vk::BlendFactor da, vk::BlendOp ao,
                   bool enable, bool separate)
        {
            s.SrcColor = sc; s.DstColor = dc; s.ColorOp = co;
            s.SrcAlpha = sa; s.DstAlpha = da; s.AlphaOp = ao;
            s.BlendEnable = enable;
            s.SepareteAlphaBlendEnable = separate; // (typo in member name, see note)
        };

    switch (mode)
    {
    case TransparencyMode::eOpaque:
        set(vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            false, false);
        break;

    case TransparencyMode::eTransparent: // straight alpha
        set(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
            vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
            true, true);
        break;

    case TransparencyMode::ePremultiplied:
        set(vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
            vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
            true, false);
        break;

    case TransparencyMode::eAdditive:
        set(vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            true, true);
        break;

    case TransparencyMode::eAlphaAdditive: // soft add
        set(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            true, true);
        break;

    case TransparencyMode::eMultiply: // S * D
        set(vk::BlendFactor::eDstColor, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            true, true);
        break;

    case TransparencyMode::eScreen: // S + D - S*D
        set(vk::BlendFactor::eOneMinusDstColor, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            vk::BlendFactor::eZero, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
            true, true);
        break;

    case TransparencyMode::eDisabled:
        set(vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
            false, false);
        break;
    }

    return s;
}

inline DepthState GetDepthStateFromTransparency(TransparencyMode::Enum mode, GraphicsPipelineDescV2& desc)
{
    DepthState state{};
    if (!desc.DeriveDepthFromTransparency)
    {
        switch (mode)
        {
        case TransparencyMode::eOpaque:
            desc.DepthMode = DepthMode::eReadWrite;
            desc.DepthCompareOp = ECompareOperation::LessOrEqual;
            break;
        default:
            desc.DepthMode = DepthMode::eReadOnly;
            desc.DepthCompareOp = ECompareOperation::LessOrEqual;
            break;
        }
    }

    return state.SetDepthTest(
        desc.DepthMode != DepthMode::eNone,
        desc.DepthMode == DepthMode::eReadWrite,
        desc.DepthCompareOp);
}

class GfxPipeline : public PipelineBase
{
public:
    GfxPipeline(SafePtr<GfxContext> ctx, const GraphicsPipelineDesc& desc);
    GfxPipeline(SafePtr<Shader> shader, GraphicsPipelineDescV2& desc);
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
