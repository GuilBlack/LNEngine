#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Utils/TypeId.h"
#include "Engine/Graphics/Resources/UniformBuffer.h"
#include "Engine/Graphics/Resources/Pipeline.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/StructsHashes.h"
#include "Engine/GlobalUtils.h"
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPass.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace lne
{
class GfxPipeline;
class GfxTechnique;
class Texture;
class ComputePipeline;
class FrameGraph;

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

class Material : public RefCountBase
{
public:
    Material(SafePtr<GfxTechnique> technique);
    ~Material();

    ShaderDomain::Enum          GetMaterialType() const { return m_MaterialType; }
    SafePtr<GfxTechnique>       GetTechnique() const;
    MaterialPassSlot            GetMaterialPassSlot(PassID passId) const;
    bool                        CanRenderToPass(PassID passId) const;

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

    SafePtr<class GfxPipeline>  GetPipeline(PassID passId, SafePtr<FrameGraph> frameGraph);

private:
    struct MaterialElement
    {
        PassID                  PassId;
        StorageBufferElement    Element;
    };

    friend class Renderer;
    using MatPassDataMap = FlatHashMap<MatPassDataHash, byte*, MatPassDataHasher>;
    using MaterialElementsMap = FlatHashMap<std::string, std::vector<MaterialElement>>;
    using TextureMap = FlatHashMap<std::string, SafePtr<Texture>>;

    SafePtr<GfxTechnique>                           m_Technique;
    MaterialElementsMap                             m_Constants;
    MatPassDataMap                                  m_PassData;
    std::mutex                                      m_DataMutex;
    FlatHashMap<PassID, MaterialPassSlot>           m_AllocatedSlots;
    FlatHashMap<MaterialPipelineHash, PipelineHandle, boost::hash<lne::MaterialPipelineHash>>       m_AllocatedPipelines;
    bool                                            m_IsTransparent{ false };
    TextureMap                                      m_Textures;
    uint32_t                                        m_DirtyFrames{ 0 };
    ShaderDomain::Enum                              m_MaterialType{ ShaderDomain::eUnknown };

private:
    template<typename T> requires std::is_trivially_copyable_v<T>
    bool SetProperty(const std::string& name, const T& value)
    {
        if (m_Constants.contains(name) == false)
            return false;
        auto& matConsts = m_Constants.at(name);
        bool success = true;
        std::lock_guard<std::mutex> lock(m_DataMutex);
        for (auto& matConst : matConsts)
        {
            if (IsOfShaderElementType(TypeIdHelper<T>::Get(), matConst.Element.Type) == false)
            {
                LNE_ERROR("Type mismatch when setting material property '{}'", name);
                success = false;
            }

            MatPassDataHash hash{
                .PassId = matConst.PassId,
                .SetIndex = matConst.Element.SetIndex,
                .Binding = matConst.Element.BindingIndex
            };
            if (m_PassData.contains(hash) == false) // normally shouldn't happen
            {
                LNE_ERROR("Pass data not found in material... what??");
                success = false;
            }
            byte* passData = m_PassData.at(hash);
            memcpy(passData + matConst.Element.Offset, &value, sizeof(T));
            InvalidateMaterial();
        }
        
        return success;
    }
    void                        InvalidateMaterial();
    bool                        IsOfShaderElementType(TypeId typeId, ShaderElementType::Enum elemType);
    bool                        CopyPassDataToBuffers(vk::CommandBuffer cmdBuffer, uint32_t frameIndex);
};


// Maybe this should be in a different file? dunno
class ComputeProgram : public RefCountBase
{
public:
    ComputeProgram(SafePtr<ComputePipeline> pipeline);
    ~ComputeProgram();

    SafePtr<ComputePipeline>    GetPipeline() const { return m_Pipeline; }

    // Set property overloads.
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, float value)
    {
        SetProperty<float>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, uint32_t value)
    {
        SetProperty<uint32_t>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, int32_t value)
    {
        SetProperty<int32_t>(cmdBuffer, name, value);
    }

    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::vec2& value)
    {
        SetProperty<glm::vec2>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::vec3& value)
    {
        SetProperty<glm::vec3>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::vec4& value)
    {
        SetProperty<glm::vec4>(cmdBuffer, name, value);
    }

    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::mat2& value)
    {
        SetProperty<glm::mat2>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::mat3& value)
    {
        SetProperty<glm::mat3>(cmdBuffer, name, value);
    }
    void                        SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const glm::mat4& value)
    {
        SetProperty<glm::mat4>(cmdBuffer, name, value);
    }

    void                        SetTexture(vk::CommandBuffer cmdBuffer, const std::string& name, SafePtr<class Texture> texture, bool isStorage = true);

    // Dispatch method: bind the compute pipeline and launch compute work.
    void Dispatch(uint32_t groupCountX,
        uint32_t groupCountY,
        uint32_t groupCountZ,
        bool async = true);

private:
    // Pointer to our compute pipeline.
    SafePtr<ComputePipeline>                                m_Pipeline;
    vk::DescriptorSet                                       m_DescriptorSet;

    // A map of uniform metadata. This should be populated during shader reflection.
    FlatHashMap<std::string, UniformElement>                m_ProgramConstants;
    FlatHashMap<uint32_t, SafePtr<UniformBuffer>>           m_UniformBuffers;
    FlatHashMap<std::string, SafePtr<Texture>>              m_Textures;

    friend class Renderer;

private:

    // Templated helper to update a uniform buffer given the name.
    template<typename T> requires std::is_trivially_copyable_v<T>
    bool SetProperty(vk::CommandBuffer cmdBuffer, const std::string& name, const T& value)
    {
        if (m_ProgramConstants.contains(name) == false)
            return false;
        auto& progConst = m_ProgramConstants.at(name);
        if (progConst.Size == sizeof(T))
            SetUniformBuffer(cmdBuffer, progConst.BindingIndex, &value, sizeof(T), progConst.Offset);
        else
            return false;
        return true;
    }

    // Function to update the uniform buffer for a given binding.
    void SetUniformBuffer(vk::CommandBuffer cmdBuffer, uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};
}
