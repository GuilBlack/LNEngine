#include "lnepch.h"
#include "Shader.h"
#include <shaderc/shaderc.hpp>

#include "GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/ApplicationBase.h"

namespace lne
{
Shader::Shader(std::shared_ptr<GfxContext> ctx, std::string_view filePath, EShaderStage stage)
    : m_Context{ ctx }, m_FilePath{ filePath }, m_Stage{stage}
{
    std::string shaderCode = ReadFile(m_FilePath + ShaderStageToExtension(stage));

    m_Code = TryCompile(shaderCode, stage);

    vk::ShaderModuleCreateInfo createInfo;
    createInfo.codeSize = m_Code.size() * sizeof(uint32_t);
    createInfo.pCode = m_Code.data();

    m_Module = m_Context->GetDevice().createShaderModule(createInfo);
}

Shader::~Shader()
{
    m_Context->GetDevice().destroyShaderModule(m_Module);
}

std::string Shader::ShaderStageToExtension(EShaderStage stage)
{
    switch (stage)
    {
    case EShaderStage::Vertex: return ".vert";
    case EShaderStage::TessellationControl: return ".tesc";
    case EShaderStage::TessellationEvaluation: return ".tese";
    case EShaderStage::Geometry: return ".geom";
    case EShaderStage::Fragment: return ".frag";
    case EShaderStage::Compute: return ".comp";
    default: return "";
    }
}

std::string Shader::ReadFile(std::string_view filePath)
{
    std::ifstream shaderSourceFile{ std::string(filePath) };
    if (!shaderSourceFile.is_open())
    {
        LNE_ERROR("Failed to open shader file: {}", filePath);
        LNE_ASSERT(false, "Failed to open shader file");
    }
    return { std::istreambuf_iterator<char>{shaderSourceFile}, {} };
}

std::vector<uint32_t> Shader::TryCompile(const std::string& sourceCode, EShaderStage stage)
{
    shaderc_shader_kind shaderKind = shaderc_glsl_infer_from_source;
    switch (stage)
    {
    case EShaderStage::Vertex:
        shaderKind = shaderc_vertex_shader;
        break;
    case EShaderStage::Fragment:
        shaderKind = shaderc_fragment_shader;
        break;
    case EShaderStage::Compute:
        shaderKind = shaderc_compute_shader;
        break;
    default:
        LNE_ASSERT(false, "Unsupported shader stage");
        break;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    constexpr bool optimize = false;
    options.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);
    options.SetGenerateDebugInfo();
    options.SetWarningsAsErrors();

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        sourceCode,
        shaderKind,
        m_FilePath.c_str(),
        options
    );

    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        LNE_ERROR("Failed to compile shader: {}", result.GetErrorMessage());
        LNE_ASSERT(false, "Failed to compile shader");
    }

    return std::vector<uint32_t>(result.begin(), result.end());
}
}

vk::ShaderStageFlagBits lne::vkut::ShaderStageToVk(EShaderStage stage)
{
    switch (stage)
    {
    case EShaderStage::Vertex: return vk::ShaderStageFlagBits::eVertex;
    case EShaderStage::TessellationControl: return vk::ShaderStageFlagBits::eTessellationControl;
    case EShaderStage::TessellationEvaluation: return vk::ShaderStageFlagBits::eTessellationEvaluation;
    case EShaderStage::Geometry: return vk::ShaderStageFlagBits::eGeometry;
    case EShaderStage::Fragment: return vk::ShaderStageFlagBits::eFragment;
    case EShaderStage::Compute: return vk::ShaderStageFlagBits::eCompute;
    default: return vk::ShaderStageFlagBits::eVertex;
    }
}
