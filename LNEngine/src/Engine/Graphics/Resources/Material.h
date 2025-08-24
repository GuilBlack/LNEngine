#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Resources/UniformBuffer.h"
#include "Engine/Graphics/Resources/Pipeline.h"
#include "Engine/Graphics/Structs.h"

namespace lne
{
class Material : public RefCountBase
{
public:
    Material(SafePtr<class GfxPipeline> pipeline, MaterialType::Enum materialType = MaterialType::eMesh);
    ~Material();

    SafePtr<class GfxPipeline> GetPipeline() const { return m_Pipeline; }
    MaterialType::Enum GetMaterialType() const { return m_MaterialType; }
    bool IsTransparent() const { return m_IsTransparent; }
    void SetTransparency(bool isTransparent) { m_IsTransparent = isTransparent; }

    void                        SetProperty(const std::string& name, float value)
    { SetProperty<float>(name, value); }

    void                        SetProperty(const std::string& name, const glm::vec2& value)
    { SetProperty<glm::vec2>(name, value); }

    void                        SetProperty(const std::string& name, const glm::vec3& value)
    { SetProperty<glm::vec3>(name, value); }
    void                        SetProperty(const std::string& name, const glm::vec4& value)
    { SetProperty<glm::vec4>(name, value); }

    void                        SetProperty(const std::string& name, const glm::mat2& value)
    { SetProperty<glm::mat2>(name, value); }
    void                        SetProperty(const std::string& name, const glm::mat3& value)
    { SetProperty<glm::mat3>(name, value); }
    void                        SetProperty(const std::string& name, const glm::mat4& value)
    { SetProperty<glm::mat4>(name, value); }

    void                        SetTexture(const std::string& name, 
                                           SafePtr<class Texture> texture);

private:
    SafePtr<class GfxPipeline> m_Pipeline;
    std::unordered_map<std::string, UniformElement> m_MaterialConstants;
    std::map<uint32_t, SafePtr<UniformBuffer>> m_UniformBuffers;
    MaterialType::Enum m_MaterialType;
    bool m_IsTransparent{ false };
    uint32_t m_CurrentFrameInFlight;
    std::vector<vk::DescriptorSet> m_DescSets;
    std::unordered_map<std::string, SafePtr<class Texture>> m_Textures;

    friend class Renderer;

private:
    template<typename T> requires std::is_trivially_copyable_v<T>
    bool SetProperty(const std::string& name, const T& value)
    {
        if (m_MaterialConstants.contains(name) == false)
            return false;
        auto& matConst = m_MaterialConstants.at(name);
        if (matConst.Size == sizeof(T))
            SetUniformBuffer(m_MaterialConstants.at(name).BindingIndex, &value, sizeof(T), matConst.Offset);
        else
            return false;
        return true;
    }
    void SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};


// Maybe this should be in a different file? dunno
class ComputeProgram : public RefCountBase
{
public:
    ComputeProgram(SafePtr<ComputePipeline> pipeline);
    ~ComputeProgram();

    SafePtr<ComputePipeline>    GetPipeline() const { return m_Pipeline; }

    // Set property overloads.
    void                        SetProperty(const std::string& name, float value)
    { SetProperty<float>(name, value); }
    void                        SetProperty(const std::string& name, uint32_t value)
    { SetProperty<uint32_t>(name, value); }
    void                        SetProperty(const std::string& name, int32_t value)
    { SetProperty<int32_t>(name, value); }

    void                        SetProperty(const std::string& name, const glm::vec2& value)
    { SetProperty<glm::vec2>(name, value); }
    void                        SetProperty(const std::string& name, const glm::vec3& value)
    { SetProperty<glm::vec3>(name, value); }
    void                        SetProperty(const std::string& name, const glm::vec4& value)
    { SetProperty<glm::vec4>(name, value); }

    void                        SetProperty(const std::string& name, const glm::mat2& value)
    { SetProperty<glm::mat2>(name, value); }
    void                        SetProperty(const std::string& name, const glm::mat3& value)
    { SetProperty<glm::mat3>(name, value); }
    void                        SetProperty(const std::string& name, const glm::mat4& value)
    { SetProperty<glm::mat4>(name, value); }

    void                        SetTexture(const std::string& name, SafePtr<class Texture> texture, bool isStorage = true);

    // Dispatch method: bind the compute pipeline and launch compute work.
    void Dispatch(uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ,
        bool async = true);

private:
    // Pointer to our compute pipeline.
    SafePtr<ComputePipeline>                                m_Pipeline;

    // A map of uniform metadata. This should be populated during shader reflection.
    std::unordered_map<std::string, UniformElement>         m_ProgramConstants;
    std::map<uint32_t, SafePtr<UniformBuffer>>              m_UniformBuffers;
    std::unordered_map<std::string, SafePtr<class Texture>> m_Textures;

    friend class Renderer;

private:

    // Templated helper to update a uniform buffer given the name.
    template<typename T> requires std::is_trivially_copyable_v<T>
    bool SetProperty(const std::string& name, const T& value)
    {
        if (m_ProgramConstants.contains(name) == false)
            return false;
        auto& progConst = m_ProgramConstants.at(name);
        if (progConst.Size == sizeof(T))
            SetUniformBuffer(progConst.BindingIndex, &value, sizeof(T), progConst.Offset);
        else
            return false;
        return true;
    }

    // Function to update the uniform buffer for a given binding.
    void SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};
}
