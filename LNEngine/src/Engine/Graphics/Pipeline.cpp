#include "lnepch.h"
#include "Pipeline.h"
#include "GfxContext.h"
#include "Framebuffer.h"
#include "Texture.h"
#include "Core/Utils/Log.h"
#include "FrameGraph/FrameGraph.h"

namespace lne
{
#pragma region PipelineBase implementation

PipelineBase::~PipelineBase()
{
    PipelineResourceDeletion pipelineDeletion{
        .Pipeline = m_Pipeline,
        .Layout = m_Layout
    };
    ResourceDeletion deletion{
        .Type = ResourceType::ePipeline,
        .Resource = pipelineDeletion
    };
    m_Context->EnqueueResourceDeletion(deletion);
}

void PipelineBase::Bind(const vk::CommandBuffer& cmdBuffer) const
{
    cmdBuffer.bindPipeline(m_BindPoint, m_Pipeline);
}

PipelineBase::PipelineBase(SafePtr<GfxContext> ctx, const std::string& name, vk::PipelineBindPoint bindPoint)
    : m_Context(ctx), m_Name(name), m_BindPoint(bindPoint)
{}

vk::PipelineLayout PipelineBase::CreatePipelineLayout(const std::vector<vk::DescriptorSetLayout>& layouts)
{
    std::vector<vk::DescriptorSetLayout> completeLayouts;
    completeLayouts.reserve(layouts.size() + 1);
    completeLayouts.insert(completeLayouts.begin(), layouts.begin(), layouts.end());
    completeLayouts.emplace_back(m_Context->GetBindlessDescriptorSetLayout());
    auto layout = m_Context->GetDevice().createPipelineLayout(vk::PipelineLayoutCreateInfo{
        {},
        completeLayouts
    });
    m_Context->SetVkObjectName(layout, std::format("PipelineLayout: {}", m_Name));
    return layout;
}

#pragma endregion

#pragma region Graphics pipeline

DepthDesc& DepthDesc::SetDepthTest(bool isEnabled, bool write, ECompareOperation compare)
{
    DepthTestEnable = isEnabled;
    DepthWriteEnable = isEnabled;
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

GfxPipeline::GfxPipeline(SafePtr<GfxContext> ctx, const GraphicsPipelineDesc& desc)
    : PipelineBase(ctx, desc.Name, vk::PipelineBindPoint::eGraphics), m_Desc(desc)
{
    static constexpr vk::PipelineVertexInputStateCreateInfo vertexInputStateInfo({}, 0, nullptr, 0, nullptr);
    static constexpr std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

    m_Shader = ctx->CreateShader(desc.PathToShaders);
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    m_AssociatedRenderPassName = m_Shader->GetHeader().RenderPass;
    m_AssociatedRenderPassNameHash = m_Shader->GetHeader().RenderPassHash;

    shaderStages.reserve(m_Shader->GetStageCount());
    std::vector<std::string> entryPoints{};
    entryPoints.reserve(m_Shader->GetStageCount());
    for (auto& [stage, module] : m_Shader->GetModules())
    {
        entryPoints.push_back(m_Shader->GetHeader().StageHeaders.at(stage).EntryPoint);
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo(
            {},
            vkut::ShaderStageToVk(stage),
            module,
            entryPoints.back().c_str()
        ));
    }

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
    const FrameGraphNode* node = desc.FrameGraph->GetNode(m_AssociatedRenderPassName);

    if (!node)
    {
        LNE_ERROR("Failed to find node with name: {}", m_AssociatedRenderPassName);
        m_Desc.FrameGraph = nullptr;
        return;
    }

    if (node->Type != RenderPassType::eGraphics)
    {
        LNE_ERROR("Node with name: {} is not a graphics node", node->Name);
        m_Desc.FrameGraph = nullptr;
        return;
    }

    std::vector<vk::Format> colorFormats;
    vk::Format depthFormat = vk::Format::eUndefined;
    for (auto& inputHandle : node->InputResources)
    {
        auto input = desc.FrameGraph->GetResource(inputHandle);
        if (input != nullptr && input->Type == FrameGraphResourceType::eAttachment)
        {
            FrameGraphResourceImageInfo imageInfo = std::get<FrameGraphResourceImageInfo>(input->Info.Variant);
            if (imageInfo.Flags & vk::ImageUsageFlagBits::eColorAttachment)
            {
                colorFormats.emplace_back(imageInfo.Format);
            }
            else if (imageInfo.Flags & vk::ImageUsageFlagBits::eDepthStencilAttachment)
            {
                depthFormat = imageInfo.Format;
            }
        }
    }

    for (auto& outputHandle : node->OutputResources)
    {
        auto output = desc.FrameGraph->GetResource(outputHandle);
        if (output != nullptr && output->Type == FrameGraphResourceType::eAttachment)
        {
            FrameGraphResourceImageInfo imageInfo = std::get<FrameGraphResourceImageInfo>(output->Info.Variant);
            if (imageInfo.Flags & vk::ImageUsageFlagBits::eColorAttachment)
            {
                colorFormats.emplace_back(imageInfo.Format);
            }
            else if (imageInfo.Flags & vk::ImageUsageFlagBits::eDepthStencilAttachment)
            {
                depthFormat = imageInfo.Format;
            }
        }
    }

    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(colorFormats.size(), blendState);

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
        vk::PipelineRenderingCreateInfo{
            0,
            colorFormats,
            depthFormat
        }
    };
    
    auto result = m_Context->GetDevice().createGraphicsPipeline(nullptr, pipelineInfoChain.get<vk::GraphicsPipelineCreateInfo>(), nullptr);

    if (result.result != vk::Result::eSuccess)
    {
        LNE_ERROR("Failed to create graphics pipeline: {}", vk::to_string(result.result));
        return;
    }
    m_Pipeline = result.value;
    m_Context->SetVkObjectName(m_Pipeline, std::format("GraphicsPipeline: {}", desc.Name));
    m_Desc.FrameGraph = nullptr;// We don't need the frame graph anymore
}

#pragma endregion

ComputePipeline::ComputePipeline(SafePtr<GfxContext> ctx, const ComputePipelineDesc& desc)
    : PipelineBase(ctx, desc.Name, vk::PipelineBindPoint::eCompute), m_Desc(desc)
{
    m_Shader = ctx->CreateShader(desc.PathToShader);
    std::string entryPoint = m_Shader->GetHeader().StageHeaders.at(ShaderStage::eCompute).EntryPoint;
    vk::PipelineShaderStageCreateInfo shaderStage = vk::PipelineShaderStageCreateInfo(
        {},
        vk::ShaderStageFlagBits::eCompute,
        m_Shader->GetModules().begin()->second,
        entryPoint.c_str()
    );

    m_Layout = CreatePipelineLayout(m_Shader->GetDescriptorSetLayouts());

    vk::ComputePipelineCreateInfo computePipelineInfo = vk::ComputePipelineCreateInfo(
        {},
        shaderStage,
        m_Layout
    );

    auto result = m_Context->GetDevice().createComputePipeline(nullptr, computePipelineInfo, nullptr);

    if (result.result != vk::Result::eSuccess)
    {
        LNE_ERROR("Failed to create compute pipeline: {}", vk::to_string(result.result));
        return;
    }
    m_Pipeline = result.value;
    m_Context->SetVkObjectName(m_Pipeline, std::format("ComputePipeline: {}", desc.Name));
}
}
