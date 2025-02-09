#include "lnepch.h"
#include "Material.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Renderer.h"
#include "Core/ApplicationBase.h"
#include "CommandBufferManager.h"
#include "DynamicDescriptorAllocator.h"
#include "Texture.h"

namespace lne
{
Material::Material(SafePtr<GfxPipeline> pipeline)
    : m_Pipeline(pipeline)
{
    DescriptorSet materialDescSet = m_Pipeline->m_Shader->GetReflectedData().DescriptorSets.at(3);

    for (const auto& [binding, ub] : materialDescSet.UniformBuffers)
        m_UniformBuffers.emplace(std::make_pair(ub.BindingIndex, UniformBuffer(m_Pipeline->m_Context, ub.Size)));

    for (const auto& [name, element] : m_Pipeline->m_Shader->GetReflectedData().UniformElements)
    {
        if (element.SetIndex == 3)
            m_MaterialConstants.emplace(name, element);
    }
}

Material::~Material()
{
    for (auto& [binding, ub] : m_UniformBuffers)
        ub.Nuke();
}

void Material::SetProperty(std::string_view name, float value)
{
    SetProperty<float>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::vec2& value)
{
    SetProperty<glm::vec2>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::vec3 & value)
{
    SetProperty<glm::vec3>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::vec4 & value)
{
    SetProperty<glm::vec4>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::mat2& value)
{
    SetProperty<glm::mat2>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::mat3& value)
{
    SetProperty<glm::mat3>(std::string(name), value);
}

void Material::SetProperty(std::string_view name, const glm::mat4& value)
{
    SetProperty<glm::mat4>(std::string(name), value);
}

void Material::SetTexture(std::string_view name, SafePtr<Texture> texture)
{
    SetProperty<uint32_t>(std::string(name), texture->GetBindlessTextureHandle());
}

void Material::SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto& ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub.CopyData(ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer(), data, size, offset);
}

//////////////////////////////////////////////////////////////////////////
// ComputeProgram implementation /////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ComputeProgram::ComputeProgram(SafePtr<ComputePipeline> pipeline)
    : m_Pipeline(pipeline)
{
    DescriptorSet materialDescSet = m_Pipeline->m_Shader->GetReflectedData().DescriptorSets.at(1);

    for (const auto& [binding, ub] : materialDescSet.UniformBuffers)
        m_UniformBuffers.emplace(std::make_pair(ub.BindingIndex, UniformBuffer(m_Pipeline->m_Context, ub.Size)));

    for (const auto& [name, element] : m_Pipeline->m_Shader->GetReflectedData().UniformElements)
    {
        if (element.SetIndex == 1)
            m_ProgramConstants.emplace(name, element);
    }
}

ComputeProgram::~ComputeProgram()
{
    for (auto& [binding, ub] : m_UniformBuffers)
        ub.Nuke();
}

void ComputeProgram::SetProperty(std::string_view name, float value)
{
    SetProperty<float>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::vec2& value)
{
    SetProperty<glm::vec2>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::vec3& value)
{
    SetProperty<glm::vec3>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::vec4& value)
{
    SetProperty<glm::vec4>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::mat2& value)
{
    SetProperty<glm::mat2>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::mat3& value)
{
    SetProperty<glm::mat3>(std::string(name), value);
}

void ComputeProgram::SetProperty(std::string_view name, const glm::mat4& value)
{
    SetProperty<glm::mat4>(std::string(name), value);
}

void ComputeProgram::SetTexture(std::string_view name, SafePtr<Texture> texture, bool isStorage)
{
    if (isStorage)
        SetProperty<uint32_t>(std::string(name), texture->GetBindlessStorageHandle());
    else
        SetProperty<uint32_t>(std::string(name), texture->GetBindlessTextureHandle());
}

void ComputeProgram::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, bool immediate)
{
    ApplicationBase::GetRenderer().Dispatch(this, groupCountX, groupCountY, groupCountZ, immediate);
}

void ComputeProgram::SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto& ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub.CopyData(ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer(), data, size, offset);
}
}
