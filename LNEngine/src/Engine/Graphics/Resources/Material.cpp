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
// ComputeProgram const std::string&
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
