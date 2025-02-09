#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/UniformBuffer.h"
#include "Engine/Graphics/Pipeline.h"
#include "Structs.h"

namespace lne
{
class Material : public RefCountBase
{
public:
    MOVABLE_ONLY(Material);
    Material(SafePtr<class GfxPipeline> pipeline);
    ~Material();

    SafePtr<class GfxPipeline> GetPipeline() const { return m_Pipeline; }

    void SetProperty(std::string_view name, float value);
    void SetProperty(std::string_view name, const glm::vec2& value);
    void SetProperty(std::string_view name, const glm::vec3& value);
    void SetProperty(std::string_view name, const glm::vec4& value);

    void SetProperty(std::string_view name, const glm::mat2& value);
    void SetProperty(std::string_view name, const glm::mat3& value);
    void SetProperty(std::string_view name, const glm::mat4& value);
    
    void SetTexture(std::string_view name, SafePtr<class Texture> texture);

private:
    SafePtr<class GfxPipeline> m_Pipeline;
    std::unordered_map<std::string, UniformElement> m_MaterialConstants;
    std::map<uint32_t, UniformBuffer> m_UniformBuffers;

    friend class Renderer;

private:
    template<typename T> requires std::is_trivially_copyable_v<T>
    void SetProperty(const std::string& name, const T& value)
    {
        if (m_MaterialConstants.contains(name) == false)
            return;
        auto& matConst = m_MaterialConstants.at(name);
        if (matConst.Size == sizeof(T))
            SetUniformBuffer(m_MaterialConstants.at(name).BindingIndex, &value, sizeof(T), matConst.Offset);
    }
    void SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};


// Maybe this should be in a different file? dunno
class ComputeProgram : public RefCountBase
{
public:
    MOVABLE_ONLY(ComputeProgram);
    ComputeProgram(SafePtr<ComputePipeline> pipeline);
    ~ComputeProgram();

    SafePtr<ComputePipeline> GetPipeline() const { return m_Pipeline; }

    // Set property overloads.
    void SetProperty(std::string_view name, float value);
    void SetProperty(std::string_view name, const glm::vec2& value);
    void SetProperty(std::string_view name, const glm::vec3& value);
    void SetProperty(std::string_view name, const glm::vec4& value);
    void SetProperty(std::string_view name, const glm::mat2& value);
    void SetProperty(std::string_view name, const glm::mat3& value);
    void SetProperty(std::string_view name, const glm::mat4& value);

    void SetTexture(std::string_view name, SafePtr<class Texture> texture, bool isStorage = true);

    // Dispatch method: bind the compute pipeline and launch compute work.
    void Dispatch(uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ,
        bool async = true);

private:
    // Pointer to our compute pipeline.
    SafePtr<ComputePipeline> m_Pipeline;

    // A map of uniform metadata. This should be populated during shader reflection.
    std::unordered_map<std::string, UniformElement> m_ProgramConstants;
    std::map<uint32_t, UniformBuffer> m_UniformBuffers;

    friend class Renderer;

private:

    // Templated helper to update a uniform buffer given the name.
    template<typename T> requires std::is_trivially_copyable_v<T>
    void SetProperty(const std::string& name, const T& value)
    {
        if (m_ProgramConstants.contains(name) == false)
            return;
        auto& progConst = m_ProgramConstants.at(name);
        if (progConst.Size == sizeof(T))
            SetUniformBuffer(progConst.BindingIndex, &value, sizeof(T), progConst.Offset);
    }

    // Function to update the uniform buffer for a given binding.
    void SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};
}
