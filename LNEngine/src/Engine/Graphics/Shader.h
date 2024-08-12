#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"
#include "Structs.h"

namespace lne
{

struct ReflectedData
{
    std::map<uint32_t, DescriptorSet> DescriptorSets;
    std::unordered_map<std::string, UniformElement> UniformElements;
};

class Shader : public RefCountBase
{
    struct ShaderHeaderInfo
    {
        std::string EntryPoint;
    };
    using Header = std::unordered_map<ShaderStage::Enum, ShaderHeaderInfo>;
public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath);
    [[nodiscard]] std::unordered_map<ShaderStage::Enum, vk::ShaderModule> GetModules() const { return m_Modules; }
    [[nodiscard]] uint32_t GetStageCount() const { return (uint32_t)m_Modules.size(); }
    [[nodiscard]] const std::vector<vk::DescriptorSetLayout>& GetDescriptorSetLayouts() const { return m_DescriptorSetLayouts; }
    virtual ~Shader();

private:
    SafePtr<class GfxContext> m_Context;
    std::string m_FilePath;
    std::string m_Name;
    std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> m_SpirvCode{};
    std::unordered_map<ShaderStage::Enum, vk::ShaderModule> m_Modules{};
    std::vector<vk::DescriptorSetLayout> m_DescriptorSetLayouts{};
    ReflectedData m_ReflectedData{};

private:
    std::string ShaderStageToExtension(ShaderStage::Enum stage);
    std::tuple<std::string, Shader::Header> ReadFile(std::string_view filePath);
    Shader::Header ParseHeader(std::string& headerSource);
    std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> CompileToSpirv(const std::string& sourceCode, Shader::Header header);
    void ReflectOnSpirv(std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode);
    std::unordered_map<ShaderStage::Enum, vk::ShaderModule> CreateModules(std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode);
    void CreateDescriptorSetLayouts();
};

namespace vkut
{
vk::ShaderStageFlagBits ShaderStageToVk(ShaderStage::Enum stage);
}

}

