#include "lnepch.h"
#include "Shader.h"
#include <shaderc/shaderc.hpp>

#include "GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Texture.h"

namespace lne
{

#pragma region Utility Functions

shaderc_shader_kind ShaderStageToShaderc(EShaderStage stage)
{
    switch (stage)
    {
    case EShaderStage::Vertex: return shaderc_glsl_vertex_shader;
    case EShaderStage::TessellationControl: return shaderc_glsl_tess_control_shader;
    case EShaderStage::TessellationEvaluation: return shaderc_glsl_tess_evaluation_shader;
    case EShaderStage::Geometry: return shaderc_glsl_geometry_shader;
    case EShaderStage::Fragment: return shaderc_glsl_fragment_shader;
    case EShaderStage::Compute: return shaderc_glsl_compute_shader;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return shaderc_glsl_vertex_shader;
}

std::string ShaderStageToDefine(EShaderStage stage)
{
    static const std::string Vertex("VERT");
    static const std::string TessellationControl("TESC");
    static const std::string TessellationEvaluation("TESE");
    static const std::string Geometry("GEOM");
    static const std::string Fragment("FRAG");
    static const std::string Compute("COMP");

    switch (stage)
    {
    case EShaderStage::Vertex: return Vertex;
    case EShaderStage::TessellationControl: return TessellationControl;
    case EShaderStage::TessellationEvaluation: return TessellationEvaluation;
    case EShaderStage::Geometry: return Geometry;
    case EShaderStage::Fragment: return Fragment;
    case EShaderStage::Compute: return Compute;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return Vertex;
}

#pragma endregion

Shader::Shader(SafePtr<class GfxContext> ctx, std::string_view filePath)
    : m_Context(ctx), m_FilePath(filePath)
{
    auto[shaderCode, shaderHeader] = ReadFile(m_FilePath);

    m_SpirvCode = CompileToSpirv(shaderCode, shaderHeader);
    m_Modules = CreateModules(m_SpirvCode);
}

Shader::~Shader()
{
    for (auto&[stage, module] : m_Modules)
        m_Context->GetDevice().destroyShaderModule(module);
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

std::tuple<std::string, Shader::Header> Shader::ReadFile(std::string_view filePath)
{
    std::ifstream shaderSourceFile{ std::string(filePath) };
    if (!shaderSourceFile.is_open())
    {
        LNE_ERROR("Failed to open shader file: {}", filePath);
        LNE_ASSERT(false, "Failed to open shader file");
    }

    std::string headerSource;
    std::getline(shaderSourceFile, headerSource);

    auto header = ParseHeader(headerSource);

    return { { std::istreambuf_iterator<char>{shaderSourceFile}, {} }, header };
}

Shader::Header Shader::ParseHeader(std::string& headerSource)
{
    static const std::string headerPrefix = std::string("//#lne_head ");

    if (headerSource.substr(0, headerPrefix.length()) != headerPrefix)
        LNE_ASSERT(false, "Ill-formed header or header not found");

    Shader::Header header{};
    headerSource = headerSource.substr(headerPrefix.length());
    std::stack<char> brackets;

    uint32_t index = 0;

    std::function<std::string(uint32_t&, std::string_view)> getEntryPoint = [](uint32_t& index, std::string_view headerSource) -> auto
    {
        index += 3;
        std::string entryPoint;
        while (headerSource[index] != ']')
        {
            entryPoint += headerSource[index];
            index++;
        }
        return entryPoint;
    };

    while (index < headerSource.length())
    {
        if (headerSource[index] == '[')
           brackets.push('[');
        else if (headerSource[index] == ']')
        {
            if (brackets.empty())
                LNE_ASSERT(false, "Mismatched brackets in shader header");
            brackets.pop();
        }
        else
        {
            if (headerSource.substr(index, 2) == "Vt")
            {
                header[EShaderStage::Vertex] = ShaderHeaderInfo{ getEntryPoint(index, headerSource) };
            }
            else if (headerSource.substr(index, 2) == "Fg")
            {
                header[EShaderStage::Fragment] = ShaderHeaderInfo{ getEntryPoint(index, headerSource) };
            }
            else
            {
                LNE_ASSERT(false, "Ill-formed header with some non-conformed tokens");
            }
            continue;
        }

        index++;
    }
    if (brackets.empty() == false)
        LNE_ASSERT(false, "Ill-formed header");

    return header;
}

std::unordered_map<EShaderStage, std::vector<uint32_t>> Shader::CompileToSpirv(const std::string& sourceCode, Shader::Header header)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    constexpr bool optimize = false;
    options.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);
    options.SetGenerateDebugInfo();
    options.SetWarningsAsErrors();
    std::unordered_map<EShaderStage, std::vector<uint32_t>> spirvCode;
    std::vector<shaderc::CompileOptions> optionsForShaders(header.size(), options);
    uint32_t optionsIndex = 0;

    for (auto& [stage, headerInfo] : header)
    {
        auto& options = optionsForShaders.back();
        options.AddMacroDefinition(ShaderStageToDefine(stage), "1");

        shaderc_shader_kind shaderKind = ShaderStageToShaderc(stage);
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
        spirvCode[stage] = { result.begin(), result.end() };
        optionsForShaders.pop_back();
    }

    return spirvCode;
}

std::unordered_map<EShaderStage, vk::ShaderModule> Shader::CreateModules(std::unordered_map<EShaderStage, std::vector<uint32_t>> spirvCode)
{
    vk::ShaderModuleCreateInfo createInfo;
    std::unordered_map<EShaderStage, vk::ShaderModule> modules;

    for (auto& [stage, code] : spirvCode)
    {
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        modules[stage] = m_Context->GetDevice().createShaderModule(createInfo);
        uint32_t offset = (uint32_t)m_FilePath.find_last_of("\\/") + 1;
        uint32_t count = (uint32_t)m_FilePath.find_last_of(".") - offset;
        std::string fileName = m_FilePath.substr(offset, count);
        m_Context->SetVkObjectName(modules[stage], std::string(ShaderStageToDefine(stage) + " " + fileName));
    }
    return modules;
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
