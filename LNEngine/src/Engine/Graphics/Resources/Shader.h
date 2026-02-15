#pragma once
#include "Engine/Graphics/Enums.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace spirv_cross
{
class Compiler;
}

namespace lne
{

struct BufferBinding
{
    u32                     SetIndex;
    u32                     BindingIndex;
    u32                     Size;
    vk::ShaderStageFlags    Stages;
};

struct DescriptorSet
{
    u32                                 SetIndex;
    FlatHashMap<std::string, BufferBinding> UniformBuffers;
    FlatHashMap<std::string, BufferBinding> StorageBuffers;
};

struct UniformElement
{
    u32                                 SetIndex;
    u32                                 BindingIndex;
    u32                                 Offset;
    u32                                 Size;
    ShaderElementType::Enum             Type;
};

struct StorageBufferElement
{
    u32                                 SetIndex;
    u32                                 BindingIndex;
    u32                                 Offset;
    u32                                 Size;
    ShaderElementType::Enum             Type;
    u32                                 ArrayStride;
};

struct StorageBufferArray
{
    u32                                 SetIndex;
    u32                                 BindingIndex;
    u32                                 ArrayStride;
    u32                                 ElementSize;
};

struct PushConstantMember
{
    u32                                 Offset;
    u32                                 Size;
    ShaderElementType::Enum             Type;
};

struct PushConstantBlock
{
    u32                                 Offset = 0;      // merged min active offset across stages
    u32                                 Size = 0;      // merged (maxEnd - minOffset) across stages
    vk::ShaderStageFlags                Stages{};
    FlatHashMap<std::string, PushConstantMember> Members; // by qualified member name
};

struct ReflectedData
{
    std::map<u32, DescriptorSet>        DescriptorSets;
    FlatHashMap<std::string, UniformElement> UniformElements;
    FlatHashMap<std::string, StorageBufferElement> StorageElements;
    FlatHashMap<std::string, StorageBufferArray> StorageArrays;
    FlatHashMap<std::string, PushConstantBlock> PushConstants; // by block name
};

class Shader : public RefCountBase
{
    struct ShaderHeaderInfo
    {
        std::string                     EntryPoint;
    };

    struct Header
    {
        std::unordered_map<ShaderStage::Enum, ShaderHeaderInfo> StageHeaders;
        ShaderDomain::Enum              ShaderDomain = ShaderDomain::eUnknown;
        std::string                     RenderPass;
        u64                             RenderPassHash;
    };

public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath);

    [[nodiscard]] FlatHashMap<ShaderStage::Enum, vk::ShaderModule> GetModules() const { return m_Modules; }

    [[nodiscard]] u32                       GetStageCount() const 
    { return (u32)m_Modules.size(); }

    [[nodiscard]] const std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayouts() const
    { return m_DescriptorSetLayouts; }
    [[nodiscard]] const std::vector<vk::PushConstantRange>& GetPushConstantRanges() const
    { return m_PushConstantRanges; }

    [[nodiscard]] const ReflectedData&      GetReflectedData() const { return m_ReflectedData; }
    [[nodiscard]] Shader::Header            GetHeader() const { return m_Header; }
    [[nodiscard]] std::string               GetName() const { return m_Name; }
    [[nodiscard]] ShaderDomain::Enum        GetShaderDomain() const { return m_Header.ShaderDomain; }
    [[nodiscard]] constexpr u32             GetSetIndex(ShaderSetIndexType::Enum type) const
    {
        if (m_Header.ShaderDomain == ShaderDomain::eUnknown || 
            (u32)m_Header.ShaderDomain >= ShaderDomain::NUM_MATERIAL_TYPES)
            return -1;
        return MatTypeInfos[m_Header.ShaderDomain].SetIndices[type];
    }
    virtual ~Shader();
     
public:
    struct MatTypeInfo
    {
        std::array<s8, ShaderSetIndexType::NUM_MATERIAL_SET_INDICES> SetIndices{ -1, -1, -1, -1, -1, -1, -1, -1 };
    };
    constexpr static std::array<MatTypeInfo, ShaderDomain::NUM_MATERIAL_TYPES> MatTypeInfos = {
        MatTypeInfo{ {  0,  4,  5,  3,  2,  1, -1, -1, } }, // eMesh
        MatTypeInfo{ {  0,  3,  4,  2, -1,  1, -1, -1, } }, // ePostProcess
        MatTypeInfo{ {  0,  4,  5,  3,  2,  1, -1, -1, } }, // eMeshlet
        MatTypeInfo{ { -1, -1,  1, -1, -1, -1,  0, -1, } }  // eCompute
    };


private:
    friend class GfxPipeline;
    friend class ComputePipeline;
    SafePtr<class GfxContext>               m_Context;
    std::string                             m_FilePath;
    std::string                             m_Name;
    FlatHashMap<ShaderStage::Enum, std::vector<u32>> m_SpirvCode{};
    FlatHashMap<ShaderStage::Enum, vk::ShaderModule> m_Modules{};
    std::vector<vk::DescriptorSetLayout>    m_DescriptorSetLayouts{};
    std::vector<vk::DescriptorSetLayout>    m_CreatedLayouts{};
    std::vector<vk::PushConstantRange>      m_PushConstantRanges{};
    ReflectedData                           m_ReflectedData{};
    Header                                  m_Header{};

private:
    std::string                             ShaderStageToExtension(ShaderStage::Enum stage);

    std::tuple<std::string, Shader::Header> ReadFile(std::string_view filePath);
    Shader::Header                          ParseHeader(std::string& headerSource);

    FlatHashMap<ShaderStage::Enum, std::vector<u32>> CompileToSpirv(const std::string& sourceCode, 
                                                                         Shader::Header header);

    void                                    ReflectOnSpirv(FlatHashMap<ShaderStage::Enum,
                                                           std::vector<u32>> spirvCode);
    u32                                     ReflectSSBOStructMembers(spirv_cross::Compiler& compiler, 
                                                                     u32 struct_type_id, 
                                                                     const std::string& prefix, 
                                                                     u32 set, u32 binding);

    FlatHashMap<ShaderStage::Enum, vk::ShaderModule> CreateModules(FlatHashMap<ShaderStage::Enum, std::vector<u32>> spirvCode);
    void                                    CreateDescriptorSetLayouts();
    void                                    MakePushConstantRange();
};

namespace vkut
{
vk::ShaderStageFlagBits                     ShaderStageToVk(ShaderStage::Enum stage);
}

}

