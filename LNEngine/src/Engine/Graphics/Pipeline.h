#pragma once
#include "Shader.h"
#include "Framebuffer.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{

#pragma region Creation structs

struct DepthDesc
{
    bool                DepthTestEnable = false;
    bool                DepthWriteEnable = false;
    ECompareOperation       DepthCompareOp = ECompareOperation::Less;
    bool                StencilTestEnable = false;

    DepthDesc& SetDepthTest(bool write, ECompareOperation compare);
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

#pragma endregion

struct GraphicsPipelineDesc
{
    std::string                         Name{};
    std::string                         PathToShaders{};
    std::unordered_set<EShaderStage>    ShaderStages{};

    // rasterization settings
    ECullMode   CullMode =              ECullMode::None;
    EWindingOrder WindingOrder =        EWindingOrder::CounterClockwise;
    EFillMode                           Fill = EFillMode::Solid;

    // depth stencil settings
    DepthDesc   Depth{};
    BlendState  Blend{};

    Framebuffer Framebuffer;

    GraphicsPipelineDesc& SetName(const std::string& name) { Name = name; return *this; }
    GraphicsPipelineDesc& AddStage(EShaderStage stage) { ShaderStages.insert(stage); return *this; }
    GraphicsPipelineDesc& SetCulling(ECullMode cullMode) { CullMode = cullMode; return *this; }
    GraphicsPipelineDesc& SetWinding(EWindingOrder front) { WindingOrder = front; return *this; }
    GraphicsPipelineDesc& SetFill(EFillMode fill) { Fill = fill; return *this; }
    GraphicsPipelineDesc& EnableDepthTest(bool enable, ECompareOperation compareOp = ECompareOperation::LessOrEqual) 
    { 
        Depth.SetDepthTest(enable, compareOp); return *this; 
    }
};

class GraphicsPipeline : public RefCountBase
{
public:
    GraphicsPipeline(SafePtr<class GfxContext> ctx, const GraphicsPipelineDesc& desc);
    virtual ~GraphicsPipeline();

    void Bind(const vk::CommandBuffer& cmdBuffer) const;

    vk::PipelineLayout CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts);

private:
    SafePtr<class GfxContext> m_Context;
    SafePtr<Shader> m_Shader{};
    vk::Pipeline m_Pipeline{};
    vk::PipelineLayout m_Layout{};
    vk::PipelineBindPoint m_BindPoint = vk::PipelineBindPoint::eGraphics;
    GraphicsPipelineDesc m_Desc{};
};
}
