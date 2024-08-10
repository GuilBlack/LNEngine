#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{

class Shader : public RefCountBase
{
    struct ShaderHeaderInfo
    {
        std::string EntryPoint;
    };
    using Header = std::unordered_map<EShaderStage, ShaderHeaderInfo>;
public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath);
    [[nodiscard]] std::unordered_map<EShaderStage, vk::ShaderModule> GetModules() const { return m_Modules; }
    [[nodiscard]] uint32_t GetStageCount() const { return (uint32_t)m_Modules.size(); }
    virtual ~Shader();

private:
    SafePtr<class GfxContext> m_Context;
    std::string m_FilePath;
    std::unordered_map<EShaderStage, std::vector<uint32_t>> m_SpirvCode{};
    std::unordered_map<EShaderStage, vk::ShaderModule> m_Modules{};

private:
    std::string ShaderStageToExtension(EShaderStage stage);
    std::tuple<std::string, Shader::Header> ReadFile(std::string_view filePath);
    Shader::Header ParseHeader(std::string& headerSource);
    std::unordered_map<EShaderStage, std::vector<uint32_t>> CompileToSpirv(const std::string& sourceCode, Shader::Header header);
    std::unordered_map<EShaderStage, vk::ShaderModule> CreateModules(std::unordered_map<EShaderStage, std::vector<uint32_t>> spirvCode);

};

namespace vkut
{
vk::ShaderStageFlagBits ShaderStageToVk(EShaderStage stage);
}

}

