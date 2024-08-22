#include "lnepch.h"
#include "Pipeline.h"
#include "GfxContext.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "Core/Utils/Log.h"

namespace lne
{

#pragma region Creation helper structs implementation

DepthDesc& DepthDesc::SetDepthTest(bool write, ECompareOperation compare)
{
    DepthTestEnable = true;
    DepthWriteEnable = write;
    DepthCompareOp = compare;
    return *this;
}

BlendState& BlendState::SetColor(vk::BlendFactor srcColor, vk::BlendFactor dstColor, vk::BlendOp colorOp)
{
    SrcColor = srcColor;
    DstColor = dstColor;
    ColorOp = colorOp;
    BlendEnable = true;
    return *this;
}

BlendState& BlendState::SetAlpha(vk::BlendFactor srcAlpha, vk::BlendFactor dstAlpha, vk::BlendOp alphaOp)
{
    SrcAlpha = srcAlpha;
    DstAlpha = dstAlpha;
    AlphaOp = alphaOp;
    SepareteAlphaBlendEnable = true;
    return *this;
}

BlendState& BlendState::SetColorWriteMask(EBlendColorWriteMask mask)
{
    ColorWriteMask = mask;
    return *this;
}

#pragma endregion

#pragma region GraphicsPipeline implementation

GfxPipeline::GfxPipeline(SafePtr<GfxContext> ctx, const GraphicsPipelineDesc& desc)
    : m_Context(ctx), m_Desc(desc)
{
    static constexpr vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo({}, 0, nullptr, 0, nullptr);
    static constexpr std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = vk::PipelineInputAssemblyStateCreateInfo()
        .setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(vk::False);

    vk::PipelineTessellationStateCreateInfo tessellationStateInfo{};

    vk::PipelineViewportStateCreateInfo viewportStateInfo = vk::PipelineViewportStateCreateInfo()
        .setViewportCount(1)
        .setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterizationStateInfo = vk::PipelineRasterizationStateCreateInfo()
        .setCullMode((vk::CullModeFlagBits)desc.CullMode)
        .setFrontFace((vk::FrontFace)desc.WindingOrder)
        .setPolygonMode((vk::PolygonMode)desc.Fill)
        .setLineWidth(1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampleStateInfo = vk::PipelineMultisampleStateCreateInfo(
        {},
        vk::SampleCountFlagBits::e1, vk::False, 1.0f,
        nullptr,
        vk::False, vk::False
    );

    vk::PipelineDepthStencilStateCreateInfo depthStencilStateInfo = vk::PipelineDepthStencilStateCreateInfo(
        {},
        desc.Depth.DepthTestEnable, desc.Depth.DepthWriteEnable, (vk::CompareOp)desc.Depth.DepthCompareOp,
        vk::False, vk::False,
        {}, {},
        0.0f, 1.0f
    );

    vk::PipelineColorBlendAttachmentState blendState = vk::PipelineColorBlendAttachmentState()
        .setColorWriteMask((vk::ColorComponentFlagBits)desc.Blend.ColorWriteMask)
        .setBlendEnable(desc.Blend.BlendEnable)
        .setSrcColorBlendFactor(desc.Blend.SrcColor)
        .setDstColorBlendFactor(desc.Blend.DstColor)
        .setColorBlendOp((vk::BlendOp)desc.Blend.ColorOp);

    if (desc.Blend.SepareteAlphaBlendEnable) 
    {
        blendState.setSrcAlphaBlendFactor(desc.Blend.SrcAlpha)
            .setDstAlphaBlendFactor(desc.Blend.DstAlpha)
            .setAlphaBlendOp((vk::BlendOp)desc.Blend.AlphaOp);
    }
    else
    {
        blendState.setSrcAlphaBlendFactor(desc.Blend.SrcColor)
            .setDstAlphaBlendFactor(desc.Blend.DstColor)
            .setAlphaBlendOp((vk::BlendOp)desc.Blend.ColorOp);
    }

    auto& colorAttachments = desc.Framebuffer.GetColorAttachments();
    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(colorAttachments.size(), blendState);

    vk::PipelineColorBlendStateCreateInfo colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo(
        {},
        desc.Blend.BlendEnable,
        vk::LogicOp::eCopy,
        (uint32_t)blendAttachments.size(),
        blendAttachments.data(),
        { 0.0f, 0.0f, 0.0f, 0.0f }
    );

    vk::PipelineDynamicStateCreateInfo dynamicStateInfo = vk::PipelineDynamicStateCreateInfo(
        {}, (uint32_t)dynamicStates.size(), dynamicStates.data());

    std::vector<vk::Format> colorFormats{};
    colorFormats.reserve(colorAttachments.size());

    for (auto& colorAttachments : colorAttachments)
        colorFormats.emplace_back(colorAttachments.Texture->GetFormat());

    vk::Format depthFormat = vk::Format::eUndefined;
    if (desc.Framebuffer.HasDepth())
        depthFormat = desc.Framebuffer.GetDepthAttachment().Texture->GetFormat();

    m_Shader = ctx->CreateShader(desc.PathToShaders);
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    shaderStages.reserve(m_Shader->GetStageCount());
    for (auto&[stage, module] : m_Shader->GetModules())
    {
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo(
            {},
            vkut::ShaderStageToVk(stage),
            module,
            "main"
        ));
    }

    m_Layout = CreatePipelineLayout(m_Shader->GetDescriptorSetLayouts());

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo = vk::GraphicsPipelineCreateInfo(
        vk::PipelineCreateFlags(),
        shaderStages,
        &vertexInputStateInfo,
        &inputAssemblyStateInfo,
        &tessellationStateInfo,
        &viewportStateInfo,
        &rasterizationStateInfo,
        &multisampleStateInfo,
        &depthStencilStateInfo,
        &colorBlendStateInfo,
        &dynamicStateInfo,
        m_Layout
    );

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineInfoChain = {
        graphicsPipelineInfo,
        vk::PipelineRenderingCreateInfo(
            0,
            colorFormats,
            depthFormat
        )
    };
    
    auto result = m_Context->GetDevice().createGraphicsPipeline(nullptr, pipelineInfoChain.get<vk::GraphicsPipelineCreateInfo>(), nullptr);

    if (result.result != vk::Result::eSuccess)
    {
        LNE_ERROR("Failed to create graphics pipeline: {}", vk::to_string(result.result));
        return;
    }
    m_Pipeline = result.value;
    m_Context->SetVkObjectName(m_Pipeline, std::format("GraphicsPipeline: {}", desc.Name));
}

GfxPipeline::~GfxPipeline()
{
    m_Context->GetDevice().destroyPipelineLayout(m_Layout);
    if (m_Pipeline)
        m_Context->GetDevice().destroyPipeline(m_Pipeline);
}

void GfxPipeline::Bind(const vk::CommandBuffer& cmdBuffer) const
{
    cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_Pipeline);
}

vk::PipelineLayout GfxPipeline::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    auto layout = m_Context->GetDevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
        {},
        layouts
    });
    m_Context->SetVkObjectName(layout, std::format("PipelineLayout: {}", m_Desc.Name));
    return layout;
}

#pragma endregion
}
