#include "lnepch.h"
#include "Shader.h"
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_common.hpp>

#include "GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Texture.h"

namespace lne
{

#pragma region Utility Functions

shaderc_shader_kind ShaderStageToShaderc(ShaderStage::Enum stage)
{
    switch (stage)
    {
    case ShaderStage::eVertex: return shaderc_glsl_vertex_shader;
    case ShaderStage::eTessellationControl: return shaderc_glsl_tess_control_shader;
    case ShaderStage::eTessellationEvaluation: return shaderc_glsl_tess_evaluation_shader;
    case ShaderStage::eGeometry: return shaderc_glsl_geometry_shader;
    case ShaderStage::eFragment: return shaderc_glsl_fragment_shader;
    case ShaderStage::eCompute: return shaderc_glsl_compute_shader;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return shaderc_glsl_vertex_shader;
}

vk::ShaderStageFlagBits ShaderStageToVk(ShaderStage::Enum stage)
{
    switch (stage)
    {
    case ShaderStage::eVertex: return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::eTessellationControl: return vk::ShaderStageFlagBits::eTessellationControl;
    case ShaderStage::eTessellationEvaluation: return vk::ShaderStageFlagBits::eTessellationEvaluation;
    case ShaderStage::eGeometry: return vk::ShaderStageFlagBits::eGeometry;
    case ShaderStage::eFragment: return vk::ShaderStageFlagBits::eFragment;
    case ShaderStage::eCompute: return vk::ShaderStageFlagBits::eCompute;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return vk::ShaderStageFlagBits::eVertex;
}

std::string ShaderStageToDefine(ShaderStage::Enum stage)
{
    static const std::string Vertex("VERT");
    static const std::string TessellationControl("TESC");
    static const std::string TessellationEvaluation("TESE");
    static const std::string Geometry("GEOM");
    static const std::string Fragment("FRAG");
    static const std::string Compute("COMP");

    switch (stage)
    {
    case ShaderStage::eVertex: return Vertex;
    case ShaderStage::eTessellationControl: return TessellationControl;
    case ShaderStage::eTessellationEvaluation: return TessellationEvaluation;
    case ShaderStage::eGeometry: return Geometry;
    case ShaderStage::eFragment: return Fragment;
    case ShaderStage::eCompute: return Compute;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return Vertex;
}

UniformElementType::Enum SpirvTypeToUniformElementType(spirv_cross::SPIRType type)
{
    if (type.columns > 1)
    {
        switch (type.columns)
        {
        case 2:
            return UniformElementType::eMatrix2x2;
        case 3:
            return UniformElementType::eMatrix3x3;
        case 4:
            return UniformElementType::eMatrix4x4;
        default:
            return UniformElementType::eUnknown;
        }
    }
    switch (type.basetype)
    {
    case spirv_cross::SPIRType::Int:
    {
        switch (type.vecsize)
        {
        case 2:
            return UniformElementType::eInt2;
        case 3:
            return UniformElementType::eInt3;
        case 4:
            return UniformElementType::eInt4;
        default:
            return UniformElementType::eInt;
        }
    }
    case spirv_cross::SPIRType::UInt:
    {
        switch (type.vecsize)
        {
        case 2:
            return UniformElementType::eUInt2;
        case 3:
            return UniformElementType::eUInt3;
        case 4:
            return UniformElementType::eUInt4;
        default:
            return UniformElementType::eUInt;
        }
    }
    case spirv_cross::SPIRType::Float:
    {
        switch (type.vecsize)
        {
        case 2:
            return UniformElementType::eFloat2;
        case 3:
            return UniformElementType::eFloat3;
        case 4:
            return UniformElementType::eFloat4;
        default:
            return UniformElementType::eFloat;
        }
    }
    default:
        return UniformElementType::eUnknown;
    }
}

#pragma endregion

Shader::Shader(SafePtr<class GfxContext> ctx, std::string_view filePath)
    : m_Context(ctx), m_FilePath(filePath)
{
    auto[shaderCode, shaderHeader] = ReadFile(m_FilePath); 


    uint32_t offset = (uint32_t)m_FilePath.find_last_of("\\/") + 1;
    uint32_t count = (uint32_t)m_FilePath.find_last_of(".") - offset;
    m_Name = m_FilePath.substr(offset, count);

    m_SpirvCode = CompileToSpirv(shaderCode, shaderHeader);
    ReflectOnSpirv(m_SpirvCode);
    m_Modules = CreateModules(m_SpirvCode);
    CreateDescriptorSetLayouts();
}

Shader::~Shader()
{
    for (auto descSetLayout : m_DescriptorSetLayouts)
        m_Context->GetDevice().destroyDescriptorSetLayout(descSetLayout);
    for (auto&[stage, module] : m_Modules)
        m_Context->GetDevice().destroyShaderModule(module);
}

std::string Shader::ShaderStageToExtension(ShaderStage::Enum stage)
{
    switch (stage)
    {
    case ShaderStage::eVertex: return ".vert";
    case ShaderStage::eTessellationControl: return ".tesc";
    case ShaderStage::eTessellationEvaluation: return ".tese";
    case ShaderStage::eGeometry: return ".geom";
    case ShaderStage::eFragment: return ".frag";
    case ShaderStage::eCompute: return ".comp";
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
                header[ShaderStage::eVertex] = ShaderHeaderInfo{ getEntryPoint(index, headerSource) };
            }
            else if (headerSource.substr(index, 2) == "Fg")
            {
                header[ShaderStage::eFragment] = ShaderHeaderInfo{ getEntryPoint(index, headerSource) };
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

std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> Shader::CompileToSpirv(const std::string& sourceCode, Shader::Header header)
{
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    constexpr bool optimize = false;
    options.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);
    options.SetWarningsAsErrors();
    std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode;
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

void Shader::ReflectOnSpirv(std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode)
{
    for (auto& [stage, code] : spirvCode)
    {
        LNE_INFO("Stage: {}", ShaderStageToDefine(stage));
        spirv_cross::Compiler compiler(code);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        for (const auto& res : resources.uniform_buffers)
        {
            uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
            uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
            spirv_cross::SPIRType type = compiler.get_type(res.base_type_id);
            uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(type);

            if (m_ReflectedData.DescriptorSets.contains(set) == false)
                m_ReflectedData.DescriptorSets[set] = DescriptorSet{ .SetIndex = set };

            if (m_ReflectedData.DescriptorSets[set].UniformBuffers.contains(res.name))
            {
                BufferBinding& uniformStages = m_ReflectedData.DescriptorSets[set].UniformBuffers[res.name];
                uniformStages.Stages = uniformStages.Stages | ShaderStageToVk(stage);
                continue;
            }
            else
            {
                m_ReflectedData.DescriptorSets[set].UniformBuffers[res.name] =
                {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .Size = bufferSize,
                    .Stages = ShaderStageToVk(stage)
                };
            }

            LNE_INFO("    Name: {}, Set: {}, Binding: {}, Size: {}", res.name, set, binding, bufferSize);
            for (uint32_t i = 0; i < type.member_types.size(); ++i)
            {
                std::string memberName = compiler.get_member_name(res.base_type_id, i);
                uint32_t offset = compiler.get_member_decoration(res.base_type_id, i, spv::DecorationOffset);
                uint32_t size = (uint32_t)compiler.get_declared_struct_member_size(type, i);
                spirv_cross::SPIRType memberType = compiler.get_type(type.member_types[i]);
                LNE_INFO("        Member: {}, Offset: {}, Size: {}, Type: {}", memberName, offset, size, UniformElementType::ToString(SpirvTypeToUniformElementType(memberType)));
                m_ReflectedData.UniformElements[memberName] = {
                        .UniformBuffer = res.name,
                        .Offset = offset,
                        .Size = size,
                        .Type = SpirvTypeToUniformElementType(memberType)
                };
            }
        }

        for (const auto& res : resources.storage_buffers)
        {
            uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
            uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
            spirv_cross::SPIRType type = compiler.get_type(res.base_type_id);
            uint32_t bufferSize = (uint32_t)compiler.get_declared_struct_size(type);

            if (m_ReflectedData.DescriptorSets.contains(set) == false)
                m_ReflectedData.DescriptorSets[set] = DescriptorSet{ .SetIndex = set };

            if (m_ReflectedData.DescriptorSets[set].StorageBuffers.contains(res.name))
            {
                BufferBinding& uniformStages = m_ReflectedData.DescriptorSets[set].StorageBuffers[res.name];
                uniformStages.Stages = uniformStages.Stages | ShaderStageToVk(stage);
                continue;
            }
            else
            {
                m_ReflectedData.DescriptorSets[set].StorageBuffers[res.name] =
                {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .Size = bufferSize,
                    .Stages = ShaderStageToVk(stage)
                };
            }

            LNE_INFO("    Name: {}, Set: {}, Binding: {}, Size: {}", res.name, set, binding, bufferSize);
        }
    }
}

std::unordered_map<ShaderStage::Enum, vk::ShaderModule> Shader::CreateModules(std::unordered_map<ShaderStage::Enum, std::vector<uint32_t>> spirvCode)
{
    vk::ShaderModuleCreateInfo createInfo;
    std::unordered_map<ShaderStage::Enum, vk::ShaderModule> modules;

    for (auto& [stage, code] : spirvCode)
    {
        createInfo.codeSize = code.size() * sizeof(uint32_t);
        createInfo.pCode = code.data();

        modules[stage] = m_Context->GetDevice().createShaderModule(createInfo);
        uint32_t offset = (uint32_t)m_FilePath.find_last_of("\\/") + 1;
        uint32_t count = (uint32_t)m_FilePath.find_last_of(".") - offset;
        std::string fileName = m_FilePath.substr(offset, count);
        m_Context->SetVkObjectName(modules[stage], std::string(ShaderStageToDefine(stage) + " " + m_Name));

        
    }
    return modules;
}

void Shader::CreateDescriptorSetLayouts()
{
    m_DescriptorSetLayouts.resize(m_ReflectedData.DescriptorSets.size());
    uint32_t layoutIndex = 0;
    for (auto&[setIndex, set] : m_ReflectedData.DescriptorSets)
    {
        vk::DescriptorSetLayoutCreateInfo descSetLayoutCI{};
        descSetLayoutCI.setBindingCount((uint32_t)set.UniformBuffers.size() + (uint32_t)set.StorageBuffers.size());
        std::vector<vk::DescriptorSetLayoutBinding> bindings{};
        bindings.reserve(descSetLayoutCI.bindingCount);
        for (auto& [name, buffer] : set.UniformBuffers)
        {
            auto stages = buffer.Stages;
            switch (setIndex)
            {
            case 0:
                stages = vk::ShaderStageFlagBits::eAllGraphics;
                break;
            }
            bindings.emplace_back(vk::DescriptorSetLayoutBinding(buffer.BindingIndex, vk::DescriptorType::eUniformBuffer, 1, stages));
        }
        for (auto& [name, buffer] : set.StorageBuffers)
        {
            auto stages = buffer.Stages;
            bindings.emplace_back(vk::DescriptorSetLayoutBinding(buffer.BindingIndex, vk::DescriptorType::eStorageBuffer, 1, stages));
        }
        descSetLayoutCI.setBindings(bindings);
        m_DescriptorSetLayouts[layoutIndex] = m_Context->GetDevice().createDescriptorSetLayout(descSetLayoutCI);
        m_Context->SetVkObjectName(m_DescriptorSetLayouts[layoutIndex], std::format("DescSetLayout {}, set: {}", m_Name, setIndex));
        layoutIndex++;
    }
}
}

vk::ShaderStageFlagBits lne::vkut::ShaderStageToVk(ShaderStage::Enum stage)
{
    switch (stage)
    {
    case ShaderStage::eVertex: return vk::ShaderStageFlagBits::eVertex;
    case ShaderStage::eTessellationControl: return vk::ShaderStageFlagBits::eTessellationControl;
    case ShaderStage::eTessellationEvaluation: return vk::ShaderStageFlagBits::eTessellationEvaluation;
    case ShaderStage::eGeometry: return vk::ShaderStageFlagBits::eGeometry;
    case ShaderStage::eFragment: return vk::ShaderStageFlagBits::eFragment;
    case ShaderStage::eCompute: return vk::ShaderStageFlagBits::eCompute;
    default: return vk::ShaderStageFlagBits::eVertex;
    }
}
