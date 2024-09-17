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
        ub.Destroy();
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
    SetProperty<uint32_t>(std::string(name), texture->GetBindlessHandle());
}

void Material::SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset)
{
    auto& ub = m_UniformBuffers.at(binding);
    auto& renderer = ApplicationBase::GetRenderer();
    ub.CopyData(ApplicationBase::GetRenderer().GetGraphicsCommandBufferManager()->GetCurrentCommandBuffer(), data, size, offset);
}
}
