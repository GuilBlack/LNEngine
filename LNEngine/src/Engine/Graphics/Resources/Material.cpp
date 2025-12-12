#include "lnepch.h"
#include "Material.h"

#include <Core/Utils/Log.h>

#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/Shader.h"
#include "Graphics/Renderer.h"
#include "Core/ApplicationBase.h"
#include "Graphics/CommandPoolManager.h"
#include "Graphics/DynamicDescriptorAllocator.h"
#include "Graphics/Resources/Texture.h"
#include "GfxTechnique.h"
#include "Graphics/Resources/StorageBuffer.h"
#include "../FrameGraph/FrameGraph.h"

namespace lne
{
//////////////////////////////////////////////////////////////////////////
// Material //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

Material::Material(SafePtr<GfxTechnique> technique)
    : m_Technique(technique)
{
    for (auto& [passId, passBinding] : technique->GetPasses())
    {
        SafePtr shader = passBinding.PassEffect->GetShader();
        uint32_t matSetIndex = shader->GetSetIndex(ShaderSetIndexType::eMaterial);
        for (const auto& [name, element] : shader->GetReflectedData().StorageArrays)
        {
            if (element.SetIndex != matSetIndex)
                continue;
            MatPassDataHash hash{
                .PassId = passId,
                .SetIndex = matSetIndex,
                .Binding = element.BindingIndex
            };
            byte* passData = new byte[element.ElementSize];
            std::memset(passData, 0, element.ElementSize * sizeof(byte));
            m_PassData.emplace(hash, passData);
        }

        for (const auto& [name, element] : shader->GetReflectedData().StorageElements)
        {
            if (element.SetIndex != matSetIndex)
                continue;
            if (m_Constants.contains(name) == false)
                m_Constants.emplace(name, std::vector{ MaterialElement{passId, element} });
            else
                m_Constants[name].emplace_back(MaterialElement{ passId, element });
        }

        m_AllocatedSlots = technique->AllocateMaterialSlots();
    }
    LNE_ASSERT(!technique->GetPasses().empty(), "Technique has no passes");
    m_MaterialType = m_Technique->GetPasses().begin()->second.PassEffect->GetShader()->GetShaderDomain();
    InvalidateMaterial();
}

Material::~Material()
{

    for (auto& [_, passData] : m_PassData)
        delete[] passData;
}

SafePtr<lne::GfxTechnique> Material::GetTechnique() const
{
    return m_Technique;
}

MaterialPassSlot Material::GetMaterialPassSlot(PassID passId) const
{
    auto it = m_AllocatedSlots.find(passId);
    if (it == m_AllocatedSlots.end())
    {
        LNE_ERROR("PassID not found in material");
        return MaterialPassSlot{};
    }
    return it->second;
}

bool Material::CanRenderToPass(PassID passId) const
{
    return m_Technique->ContainsPass(passId);
}

void Material::SetTexture(const std::string& name, SafePtr<Texture> texture)
{
    bool success = SetProperty<uint32_t>(name, texture->GetBindlessTextureHandle());
    if (!success)
    {
        LNE_WARN("Texture property '{}' not found in material", name);
        return;
    }
    if (m_Textures.contains(name))
        m_Textures.at(name) = texture;
    else
        m_Textures.emplace(name, texture);
}

void Material::InvalidateMaterial()
{
    auto& renderer = ApplicationBase::GetRenderer();
    if (m_DirtyFrames == 0)
        renderer.AddDirtyMaterial(SafePtr(this));
    m_DirtyFrames = renderer.GetGfxContext()->GetMaxFramesInFlight();
}

bool Material::IsOfShaderElementType(TypeId typeId, ShaderElementType::Enum elemType)
{
    static const std::unordered_map<TypeId, ShaderElementType::Enum> typeMap = {
        { TypeIdHelper<float>::Get(), ShaderElementType::eFloat },
        { TypeIdHelper<glm::vec2>::Get(), ShaderElementType::eFloat2 },
        { TypeIdHelper<glm::vec3>::Get(), ShaderElementType::eFloat3 },
        { TypeIdHelper<glm::vec4>::Get(), ShaderElementType::eFloat4 },
        { TypeIdHelper<int32_t>::Get(), ShaderElementType::eInt },
        { TypeIdHelper<glm::ivec2>::Get(), ShaderElementType::eInt2 },
        { TypeIdHelper<glm::ivec3>::Get(), ShaderElementType::eInt3 },
        { TypeIdHelper<glm::ivec4>::Get(), ShaderElementType::eInt4 },
        { TypeIdHelper<uint32_t>::Get(), ShaderElementType::eUInt },
        { TypeIdHelper<glm::uvec2>::Get(), ShaderElementType::eUInt2 },
        { TypeIdHelper<glm::uvec3>::Get(), ShaderElementType::eUInt3 },
        { TypeIdHelper<glm::uvec4>::Get(), ShaderElementType::eUInt4 },
        { TypeIdHelper<glm::mat2>::Get(), ShaderElementType::eMatrix2x2 },
        { TypeIdHelper<glm::mat3>::Get(), ShaderElementType::eMatrix3x3 },
        { TypeIdHelper<glm::mat4>::Get(), ShaderElementType::eMatrix4x4 },
    };
    if (typeMap.contains(typeId) == false)
        return false;
    return typeMap.at(typeId) == elemType;
}

bool Material::CopyPassDataToBuffers(vk::CommandBuffer cmdBuffer, uint32_t frameIndex)
{
    bool success = true;
    std::lock_guard<std::mutex> lock(m_DataMutex);
    for (auto& [hash, passData] : m_PassData)
    {
        auto effect = m_Technique->GetPassEffect(hash.PassId);
        if (effect->CopyMaterialDataToBuffer(cmdBuffer, frameIndex, 
                                         m_AllocatedSlots[hash.PassId].Slot, hash.Binding, 
                                         passData) == false)
            success = false;
    }
    return success;
}

lne::SafePtr<class GfxPipeline> Material::GetPipeline(PassID passId, SafePtr<FrameGraph> frameGraph)
{
    MaterialPipelineHash hash{
        .PassId = passId,
        .FrameGraphHash = (uint64_t)frameGraph.GetPtr()
    };
    auto it = m_AllocatedPipelines.find(hash);
    if (it != m_AllocatedPipelines.end())
        return m_Technique->GetPipeline(passId, it->second);
    auto handle = m_Technique->CreateOrGetPipeline(passId, frameGraph);
    m_AllocatedPipelines.emplace(hash, handle);
    return m_Technique->GetPipeline(passId, handle);
}

//////////////////////////////////////////////////////////////////////////
// ComputeProgram ////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ComputeProgram::ComputeProgram(SafePtr<ComputePipeline> pipeline)
    : m_Pipeline(pipeline)
{
    DescriptorSet materialDescSet = m_Pipeline->m_Shader->GetReflectedData().DescriptorSets.at(0);

    for (const auto& [binding, ub] : materialDescSet.UniformBuffers)
        m_UniformBuffers.emplace(std::make_pair(ub.BindingIndex, SafePtr(lnnew UniformBuffer(m_Pipeline->m_Context, ub.Size))));

    for (const auto& [name, element] : m_Pipeline->m_Shader->GetReflectedData().UniformElements)
    {
        if (element.SetIndex == 0)
            m_ProgramConstants.emplace(name, element);
    }
    std::vector<vk::WriteDescriptorSet> progWriteDescriptorSets;
    std::vector<vk::DescriptorBufferInfo> progUbInfo;
    progUbInfo.reserve(m_UniformBuffers.size());
    auto context = m_Pipeline->GetContext();

    m_DescriptorSet = context->AllocateDescriptorSet(
        pipeline->GetDescriptorSetLayouts()[0],
        DescriptorType::eUniformOnly
    );

    for (const auto& [binding, ub] : m_UniformBuffers)
    {
        progUbInfo.emplace_back(ub->GetDescriptorInfo());
        progWriteDescriptorSets.emplace_back(vk::WriteDescriptorSet{
            m_DescriptorSet,
            binding,
            0,
            1,
            vk::DescriptorType::eUniformBuffer,
            nullptr,
            &progUbInfo.back(),
            nullptr
        });
    }
    context->GetDevice().updateDescriptorSets(progWriteDescriptorSets, nullptr);
}

ComputeProgram::~ComputeProgram()
{
    DescriptorSetDeletion descSetDeletion{
        .Type = DescriptorType::eUniformOnly,
        .DescriptorSet = m_DescriptorSet
    };
    ResourceDeletion resourceDeletion{
        .Type = ResourceType::eDescriptorSet,
        .Resource = descSetDeletion
    };
    ApplicationBase::GetRenderer().GetGfxContext()->EnqueueResourceDeletion(resourceDeletion);
}

void ComputeProgram::SetTexture(vk::CommandBuffer cmdBuffer, const std::string& name, SafePtr<Texture> texture, bool isStorage)
{
    bool success = false;
    if (isStorage)
        success = SetProperty<uint32_t>(cmdBuffer, std::string(name), texture->GetBindlessStorageHandle());
    else
        success = SetProperty<uint32_t>(cmdBuffer, std::string(name), texture->GetBindlessTextureHandle());

    if (!success)
    {
        LNE_WARN("Texture property '{}' not found in material", name);
        return;
    }
    if (m_Textures.contains(name))
        m_Textures.at(name) = texture;
    else
        m_Textures.emplace(name, texture);
}

void ComputeProgram::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, bool immediate)
{
    ApplicationBase::GetRenderer().Dispatch(this, groupCountX, groupCountY, groupCountZ, immediate);
}


// TODO: make sure we use it just once instead of updating it for every single changes in the compute program
void ComputeProgram::SetUniformBuffer(vk::CommandBuffer cmdBuffer, uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub->CopyData(cmdBuffer, data, size, offset);
}

}
