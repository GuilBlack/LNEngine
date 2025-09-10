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
Material::Material(SafePtr<GfxPipeline> pipeline, ShaderDomain::Enum materialType)
    : m_Pipeline(pipeline), m_MaterialType(materialType)
{
    DescriptorSet materialDescSet = {};

    switch (materialType)
    {
    case ShaderDomain::eMesh:
    {
        materialDescSet = m_Pipeline->m_Shader->GetReflectedData().DescriptorSets.at(3);
        for (const auto& [name, element] :
            m_Pipeline->m_Shader->GetReflectedData().UniformElements)
        {
            if (element.SetIndex == 3)
                m_MaterialConstants.emplace(name, element);
        }
        break;
    }
    case ShaderDomain::ePostProcess:
    {
        materialDescSet = m_Pipeline->m_Shader->GetReflectedData().DescriptorSets.at(2);
        for (const auto& [name, element] :
            m_Pipeline->m_Shader->GetReflectedData().UniformElements)
        {
            if (element.SetIndex == 2)
                m_MaterialConstants.emplace(name, element);
        }
        break;
    }
    case ShaderDomain::eUnknown:
    default:
        LNE_ASSERT(false, "Material type not supported");
        break;
    }
    
    for (const auto& [binding, ub] : materialDescSet.UniformBuffers)
        m_UniformBuffers.emplace(std::make_pair(ub.BindingIndex, SafePtr(lnnew UniformBuffer(m_Pipeline->m_Context, ub.Size))));
    m_DescSets.resize(m_Pipeline->GetContext()->GetMaxFramesInFlight());
}

Material::~Material()
{
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

// TODO: make sure we use it just once instead of updating it for every single changes in the material
void Material::SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub->CopyData(ApplicationBase::GetRenderer().GetGfxContext()->GetPrimaryCommandBuffer(), data, size, offset);
}

//////////////////////////////////////////////////////////////////////////
// MaterialV2 ////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

MaterialV2::MaterialV2(SafePtr<GfxTechnique> technique)
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
            m_PassData.emplace(hash, passData);
        }

        for (const auto& [name, element] : shader->GetReflectedData().StorageElements)
        {
            if (element.SetIndex != matSetIndex)
                continue;
            m_Constants.emplace(name, MaterialElement{passId, element});
        }

        m_AllocatedSlots = technique->AllocateMaterialSlots();
    }
    m_MaterialType = m_Technique->GetPasses().begin()->second.PassEffect->GetShader()->GetShaderDomain();
}

MaterialV2::~MaterialV2()
{

    for (auto& [_, passData] : m_PassData)
        delete[] passData;
}

SafePtr<lne::GfxTechnique> MaterialV2::GetTechnique() const
{
    return m_Technique;
}

MaterialPassSlot MaterialV2::GetMaterialPassSlot(PassID passId) const
{
    auto it = m_AllocatedSlots.find(passId);
    if (it == m_AllocatedSlots.end())
    {
        LNE_ERROR("PassID not found in material");
        return MaterialPassSlot{};
    }
    return it->second;
}

void MaterialV2::SetTexture(const std::string& name, SafePtr<Texture> texture)
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

void MaterialV2::InvalidateMaterial()
{
    auto& renderer = ApplicationBase::GetRenderer();
    if (m_DirtyFrames == 0)
        renderer.AddDirtyMaterial(SafePtr(this));
    m_DirtyFrames = renderer.GetGfxContext()->GetMaxFramesInFlight();
}

bool MaterialV2::IsOfShaderElementType(TypeId typeId, ShaderElementType::Enum elemType)
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

bool MaterialV2::CopyPassDataToBuffers(vk::CommandBuffer cmdBuffer, uint32_t frameIndex)
{
    bool success = true;
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

lne::SafePtr<class GfxPipeline> MaterialV2::GetPipeline(PassID passId, SafePtr<FrameGraph> frameGraph)
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
}

ComputeProgram::~ComputeProgram()
{
}

void ComputeProgram::SetTexture(const std::string& name, SafePtr<Texture> texture, bool isStorage)
{
    bool success = false;
    if (isStorage)
        success = SetProperty<uint32_t>(std::string(name), texture->GetBindlessStorageHandle());
    else
        success = SetProperty<uint32_t>(std::string(name), texture->GetBindlessTextureHandle());

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
void ComputeProgram::SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub->CopyData(ApplicationBase::GetRenderer().GetGfxContext()->GetPrimaryCommandBuffer(), data, size, offset);
}

}
