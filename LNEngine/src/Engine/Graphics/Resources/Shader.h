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
    uint32_t                SetIndex;
    uint32_t                BindingIndex;
    uint32_t                Size;
    vk::ShaderStageFlags    Stages;
};

struct DescriptorSet
{
    uint32_t                                SetIndex;
    FlatHashMap<std::string, BufferBinding> UniformBuffers;
    FlatHashMap<std::string, BufferBinding> StorageBuffers;
};

struct UniformElement
{
    uint32_t                                SetIndex;
    uint32_t                                BindingIndex;
    uint32_t                                Offset;
    uint32_t                                Size;
    ShaderElementType::Enum                 Type;
};

struct StorageBufferElement
{
    uint32_t                                SetIndex;
    uint32_t                                BindingIndex;
    uint32_t                                Offset;
    uint32_t                                Size;
    ShaderElementType::Enum                 Type;
    uint32_t                                ArrayStride;
};

struct StorageBufferArray
{
    uint32_t                                SetIndex;
    uint32_t                                BindingIndex;
    uint32_t                                ArrayStride;
    uint32_t                                ElementSize;
};

struct PushConstantMember
{
    uint32_t Offset;
    uint32_t Size;
    ShaderElementType::Enum Type;
};

struct PushConstantBlock
{
    uint32_t Offset = 0;      // merged min active offset across stages
    uint32_t Size = 0;      // merged (maxEnd - minOffset) across stages
    vk::ShaderStageFlags Stages{};
    FlatHashMap<std::string, PushConstantMember> Members; // by qualified member name
};

struct ReflectedData
{
    std::map<uint32_t, DescriptorSet> DescriptorSets;
    FlatHashMap<std::string, UniformElement> UniformElements;
    FlatHashMap<std::string, StorageBufferElement> StorageElements;
    FlatHashMap<std::string, StorageBufferArray> StorageArrays;
    FlatHashMap<std::string, PushConstantBlock> PushConstants; // by block name
};

class Shader : public RefCountBase
{
    struct ShaderHeaderInfo
    {
        std::string EntryPoint;
    };

    struct Header
    {
        std::unordered_map<ShaderStage::Enum, ShaderHeaderInfo> StageHeaders;
        ShaderDomain::Enum ShaderDomain = ShaderDomain::eUnknown;
        std::string RenderPass;
        uint64_t RenderPassHash;
    };

public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath);

    [[nodiscard]] FlatHashMap<ShaderStage::Enum, vk::ShaderModule> GetModules() const { return m_Modules; }

    [[nodiscard]] uint32_t                  GetStageCount() const 
    { return (uint32_t)m_Modules.size(); }

    [[nodiscard]] const std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayouts() const
    { return m_DescriptorSetLayouts; }
    [[nodiscard]] const std::vector<vk::PushConstantRange>& GetPushConstantRanges() const
    { return m_PushConstantRanges; }

    [[nodiscard]] const ReflectedData&      GetReflectedData() const { return m_ReflectedData; }
    [[nodiscard]] Shader::Header            GetHeader() const { return m_Header; }
    [[nodiscard]] std::string               GetName() const { return m_Name; }
    [[nodiscard]] ShaderDomain::Enum        GetShaderDomain() const { return m_Header.ShaderDomain; }
    [[nodiscard]] constexpr uint32_t        GetSetIndex(ShaderSetIndexType::Enum type) const
    {
        if (m_Header.ShaderDomain == ShaderDomain::eUnknown || 
            (uint32_t)m_Header.ShaderDomain >= ShaderDomain::NUM_MATERIAL_TYPES)
            return -1;
        return MatTypeInfos[m_Header.ShaderDomain].SetIndices[type];
    }
    virtual ~Shader();
     
public:
    struct MatTypeInfo
    {
        std::array<int8_t, ShaderSetIndexType::NUM_MATERIAL_SET_INDICES> SetIndices{-1,-1,-1,-1,-1};
    };
    constexpr static std::array<MatTypeInfo, ShaderDomain::NUM_MATERIAL_TYPES> MatTypeInfos = {
        MatTypeInfo{ {  0,  3,  4,  2,  1 } }, // eMesh
        MatTypeInfo{ {  0,  2,  3,  1, -1 } }, // ePostProcess
    };

private:
    friend class GfxPipeline;
    friend class ComputePipeline;
    SafePtr<class GfxContext> m_Context;
    std::string m_FilePath;
    std::string m_Name;
    FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> m_SpirvCode{};
    FlatHashMap<ShaderStage::Enum, vk::ShaderModule> m_Modules{};
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts{};
    std::vector<vk::DescriptorSetLayout> m_CreatedLayouts{};
    std::vector<vk::PushConstantRange> m_PushConstantRanges{};
    ReflectedData m_ReflectedData{};
    Header m_Header{};

private:
    std::string                             ShaderStageToExtension(ShaderStage::Enum stage);

    std::tuple<std::string, Shader::Header> ReadFile(std::string_view filePath);
    Shader::Header                          ParseHeader(std::string& headerSource);

    FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> CompileToSpirv(const std::string& sourceCode, 
                                                                         Shader::Header header);

    void                                    ReflectOnSpirv(FlatHashMap<ShaderStage::Enum,
                                                           std::vector<uint32_t>> spirvCode);
    uint32_t                                ReflectSSBOStructMembers(spirv_cross::Compiler& compiler, 
                                                                     uint32_t struct_type_id, 
                                                                     const std::string& prefix, 
                                                                     uint32_t set, uint32_t binding);

    FlatHashMap<ShaderStage::Enum, vk::ShaderModule> CreateModules(FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> spirvCode);
    void                                    CreateDescriptorSetLayouts();
    void                                    MakePushConstantRange();
};

namespace vkut
{
vk::ShaderStageFlagBits ShaderStageToVk(ShaderStage::Enum stage);
}

}

