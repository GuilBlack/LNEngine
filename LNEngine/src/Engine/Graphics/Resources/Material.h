#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Utils/TypeId.h"
#include "Engine/Graphics/Resources/UniformBuffer.h"
#include "Engine/Graphics/Resources/Pipeline.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/GlobalUtils.h"
#include "Engine/Graphics/FrameGraph/RenderPass/IRenderPass.h"

namespace lne
{
class GfxPipeline;
class GfxTechnique;
class Texture;
class ComputePipeline;

class Material : public RefCountBase
{
public:
    Material(SafePtr<GfxPipeline> pipeline, ShaderDomain::Enum materialType = ShaderDomain::eMesh);
    ~Material();

    SafePtr<class GfxPipeline> GetPipeline() const { return m_Pipeline; }
    ShaderDomain::Enum GetMaterialType() const { return m_MaterialType; }
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
                                           SafePtr<Texture> texture);

private:
    SafePtr<class GfxPipeline> m_Pipeline;
    std::unordered_map<std::string, UniformElement> m_MaterialConstants;
    std::map<uint32_t, SafePtr<UniformBuffer>> m_UniformBuffers;
    ShaderDomain::Enum m_MaterialType;
    bool m_IsTransparent{ false };
    uint32_t m_CurrentFrameInFlight;
    std::vector<vk::DescriptorSet> m_DescSets;
    std::unordered_map<std::string, SafePtr<Texture>> m_Textures;

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

struct MatPassDataHash
{
    PassID          PassId;
    uint32_t        SetIndex;
    uint32_t        Binding;

    bool operator==(const MatPassDataHash& other) const
    {
        return PassId == other.PassId && SetIndex == other.SetIndex && Binding == other.Binding;
    }
};

struct MatPassDataHasher
{
    std::size_t operator()(const MatPassDataHash& k) const
    {
        size_t seed = 0;
        GlobalUtils::HashCombine(seed, k.PassId);
        GlobalUtils::HashCombine(seed, k.SetIndex);
        GlobalUtils::HashCombine(seed, k.Binding);
        return seed;
    }
};

class MaterialV2 : public RefCountBase
{
public:
    MaterialV2(SafePtr<GfxTechnique> technique);
    ~MaterialV2();

    ShaderDomain::Enum          GetMaterialType() const;
    SafePtr<GfxTechnique>       GetTechnique() const;
    MaterialPassSlot            GetMaterialPassSlot(PassID passId) const;

    bool                        SetProperty(const std::string& name, uint32_t value)
    { return SetProperty<uint32_t>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::uvec2& value)
    { return SetProperty<glm::uvec2>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::uvec3& value)
    { return SetProperty<glm::uvec3>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::uvec4& value)
    { return SetProperty<glm::uvec4>(name, value); }

    bool                        SetProperty(const std::string& name, int32_t value)
    { return SetProperty<int32_t>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::ivec2& value)
    { return SetProperty<glm::ivec2>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::ivec3& value)
    { return SetProperty<glm::ivec3>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::ivec4& value)
    { return SetProperty<glm::ivec4>(name, value); }

    bool                        SetProperty(const std::string& name, float value)
    { return SetProperty<float>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::vec2& value)
    { return SetProperty<glm::vec2>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::vec3& value)
    { return SetProperty<glm::vec3>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::vec4& value)
    { return SetProperty<glm::vec4>(name, value); }

    bool                        SetProperty(const std::string& name, const glm::mat2& value)
    { return SetProperty<glm::mat2>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::mat3& value)
    { return SetProperty<glm::mat3>(name, value); }
    bool                        SetProperty(const std::string& name, const glm::mat4& value)
    { return SetProperty<glm::mat4>(name, value); }

    void SetTexture(const std::string& name, SafePtr<Texture> texture);

private:
    struct MaterialElement
    {
        PassID                  PassId;
        StorageBufferElement    Element;
    };

    friend class Renderer;
    using MatPassDataMap = std::unordered_map<MatPassDataHash, byte*, MatPassDataHasher>;
    using MaterialElementMap = std::unordered_map<std::string, MaterialElement>;
    using TextureMap = std::unordered_map<std::string, SafePtr<Texture>>;

    SafePtr<GfxTechnique>                           m_Technique;
    MaterialElementMap                              m_Constants;
    MatPassDataMap                                  m_PassData;
    std::vector<MaterialPassSlot>                   m_AllocatedSlots;
    bool                                            m_IsTransparent{ false };
    TextureMap                                      m_Textures;
    uint32_t                                        m_DirtyFrames{ 0 };

private:
    template<typename T> requires std::is_trivially_copyable_v<T>
    bool SetProperty(const std::string& name, const T& value)
    {
        if (m_Constants.contains(name) == false)
            return false;
        auto& matConst = m_Constants.at(name);
        if (IsOfShaderElementType(TypeIdHelper<T>::Get(), matConst.Element.Type) == false)
            return false;

        MatPassDataHash hash{
            .PassId = matConst.PassId,
            .SetIndex = matConst.Element.SetIndex,
            .Binding = matConst.Element.BindingIndex
        };
        if (m_PassData.contains(hash) == false)
            return false;
        byte* passData = m_PassData.at(hash);
        memcpy(passData + matConst.Element.Offset, &value, sizeof(T));
        InvalidateMaterial();
        return true;
    }
    void InvalidateMaterial();
    bool IsOfShaderElementType(TypeId typeId, ShaderElementType::Enum elemType);
    void CopyPassDataToBuffers(vk::CommandBuffer cmdBuffer, uint32_t frameIndex);
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
    std::unordered_map<std::string, SafePtr<Texture>>       m_Textures;

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
