#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{

class Shader : public RefCountBase
{
public:
    Shader(SafePtr<class GfxContext> ctx, std::string_view filePath, EShaderStage stage);
    vk::ShaderModule GetModule() const { return m_Module; }
    EShaderStage GetStage() const { return m_Stage; }
    virtual ~Shader();

private:
    SafePtr<class GfxContext> m_Context;
    std::string m_FilePath;
    EShaderStage m_Stage;
    std::vector<uint32_t> m_Code;
    vk::ShaderModule m_Module;

private:
    std::string ShaderStageToExtension(EShaderStage stage);
    std::string ReadFile(std::string_view filePath);
    std::vector<uint32_t> TryCompile(const std::string& sourceCode, EShaderStage stage);
};

namespace vkut
{
vk::ShaderStageFlagBits ShaderStageToVk(EShaderStage stage);
}

}

