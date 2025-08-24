#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"
#include "Structs.h"

namespace spirv_cross
{
class Compiler;
}

namespace lne
{

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

struct ReflectedData
{
    std::map<uint32_t, DescriptorSet> DescriptorSets;
    std::unordered_map<std::string, UniformElement> UniformElements;
    std::unordered_map<std::string, StorageBufferElement> StorageElements;
    std::unordered_map<std::string, StorageBufferArray> StorageArrays;
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
        MaterialType::Enum MaterialType = MaterialType::eUnknown;
        std::string RenderPass;
        uint64_t RenderPassHash;
    };

public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath);

    [[nodiscard]] std::unordered_map<ShaderStage::Enum, vk::ShaderModule> GetModules() const { return m_Modules; }

    [[nodiscard]] uint32_t                  GetStageCount() const 
    { return (uint32_t)m_Modules.size(); }

    [[nodiscard]] const std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayouts() const
    { return m_DescriptorSetLayouts; }

    [[nodiscard]] const ReflectedData&      GetReflectedData() const { return m_ReflectedData; }
    [[nodiscard]] Shader::Header            GetHeader() const { return m_Header; }
    [[nodiscard]] std::string               GetName() const { return m_Name; }
    virtual ~Shader();
     
public:
    struct MatTypeInfo
    {
        std::array<int8_t, MaterialSetIndexType::NUM_MATERIAL_SET_INDICES> SetIndices{-1,-1,-1,-1,-1};
    };
    constexpr static std::array<MatTypeInfo, MaterialType::NUM_MATERIAL_TYPES> MatTypeInfos = {
        MatTypeInfo{ {  0,  3,  4,  2,  1 } }, // eMesh
        MatTypeInfo{ {  0,  2,  3,  1, -1 } }, // ePostProcess
    };

private:
    SafePtr<class GfxContext> m_Context;
    std::string m_FilePath;
    std::string m_Name;
    std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> m_SpirvCode{};
    std::unordered_map<ShaderStage::Enum, vk::ShaderModule> m_Modules{};
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts{};
    std::vector<vk::DescriptorSetLayout> m_CreatedLayouts{};
    ReflectedData m_ReflectedData{};
    Header m_Header{};

private:
    std::string                             ShaderStageToExtension(ShaderStage::Enum stage);

    std::tuple<std::string, Shader::Header> ReadFile(std::string_view filePath);
    Shader::Header                          ParseHeader(std::string& headerSource);

    std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> CompileToSpirv(const std::string& sourceCode, Shader::Header header);

    void                                    ReflectOnSpirv(std::unordered_map<ShaderStage::Enum,
                                               std::vector<uint32_t>> spirvCode);
    void                                    ReflectSSBOStructMembers(spirv_cross::Compiler& compiler, 
                                                                     uint32_t struct_type_id, 
                                                                     const std::string& prefix, 
                                                                     uint32_t set, uint32_t binding);

    std::unordered_map<ShaderStage::Enum, vk::ShaderModule> CreateModules(std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode);
    void                                    CreateDescriptorSetLayouts();
};

namespace vkut
{
vk::ShaderStageFlagBits ShaderStageToVk(ShaderStage::Enum stage);
}

}

