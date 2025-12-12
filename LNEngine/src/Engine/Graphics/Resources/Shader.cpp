#include "lnepch.h"
#include "Shader.h"
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_common.hpp>

#include "Graphics/GfxContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Resources/Texture.h"
#include "Resources/ShaderFileIncluder.h"
#include "Graphics/Renderer.h"

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
    case ShaderStage::eMesh: return shaderc_glsl_mesh_shader;
    case ShaderStage::eTask: return shaderc_glsl_task_shader;
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
    case ShaderStage::eMesh: return vk::ShaderStageFlagBits::eMeshEXT;
    case ShaderStage::eTask: return vk::ShaderStageFlagBits::eTaskEXT;
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return vk::ShaderStageFlagBits::eVertex;
}

std::string ShaderStageToDefine(ShaderStage::Enum stage)
{
    switch (stage)
    {
    case ShaderStage::eVertex: return "VERT";
    case ShaderStage::eTessellationControl: return "TESC";
    case ShaderStage::eTessellationEvaluation: return "TESE";
    case ShaderStage::eGeometry: return "GEOM";
    case ShaderStage::eFragment: return "FRAG";
    case ShaderStage::eCompute: return "COMP";
    case ShaderStage::eMesh: return "MESH";
    case ShaderStage::eTask: return "TASK";
    default: LNE_ASSERT(false, "This isn't a stage");
    }
    return "VERT";
}

ShaderElementType::Enum SpirvTypeToUniformElementType(spirv_cross::SPIRType type)
{
    if (type.columns > 1)
    {
        switch (type.columns)
        {
        case 2:
            return ShaderElementType::eMatrix2x2;
        case 3:
            return ShaderElementType::eMatrix3x3;
        case 4:
            return ShaderElementType::eMatrix4x4;
        default:
            return ShaderElementType::eUnknown;
        }
    }
    switch (type.basetype)
    {
    case spirv_cross::SPIRType::Int:
    {
        switch (type.vecsize)
        {
        case 2:
            return ShaderElementType::eInt2;
        case 3:
            return ShaderElementType::eInt3;
        case 4:
            return ShaderElementType::eInt4;
        default:
            return ShaderElementType::eInt;
        }
    }
    case spirv_cross::SPIRType::UInt:
    {
        switch (type.vecsize)
        {
        case 2:
            return ShaderElementType::eUInt2;
        case 3:
            return ShaderElementType::eUInt3;
        case 4:
            return ShaderElementType::eUInt4;
        default:
            return ShaderElementType::eUInt;
        }
    }
    case spirv_cross::SPIRType::Float:
    {
        switch (type.vecsize)
        {
        case 2:
            return ShaderElementType::eFloat2;
        case 3:
            return ShaderElementType::eFloat3;
        case 4:
            return ShaderElementType::eFloat4;
        default:
            return ShaderElementType::eFloat;
        }
    }
    default:
        return ShaderElementType::eUnknown;
    }
}

ShaderStage::Enum MapShaderToken(std::string_view token)
{
    if (token == "Vt")
        return ShaderStage::eVertex;
    if (token == "Fg")
        return ShaderStage::eFragment;
    if (token == "Gm")
        return ShaderStage::eGeometry;
    if (token == "Tc")
        return ShaderStage::eTessellationControl;
    if (token == "Te")
        return ShaderStage::eTessellationEvaluation;
    if (token == "Cp")
        return ShaderStage::eCompute;
    if (token == "Ms")
        return ShaderStage::eMesh;
    if (token == "Ts")
        return ShaderStage::eTask;
    return ShaderStage::eUnknown;
}

ShaderDomain::Enum MapMaterialTypeToken(std::string_view token)
{
    if (token == "Mesh")
        return ShaderDomain::eMesh;
    if (token == "PostProcess")
        return ShaderDomain::ePostProcess;
    if (token == "Meshlet")
        return ShaderDomain::eMeshlet;
    return ShaderDomain::eUnknown;
}

#pragma endregion

#pragma region Saving Utilities
struct PackedSpvHeader
{
    // magiv value = LSPV
    constexpr static uint32_t   MagicValue = ('L' << 24) | ('S' << 16) | ('P' << 8) | 'V';
    uint32_t                    Magic;
    uint32_t                    StageCount;
    uint32_t                    HeadersCount; // Number of glslh.

    void Serialize(std::ostream& os) const
    {
        os.write(reinterpret_cast<const char*>(&Magic), sizeof(Magic));
        os.write(reinterpret_cast<const char*>(&StageCount), sizeof(StageCount));
        os.write(reinterpret_cast<const char*>(&HeadersCount), sizeof(HeadersCount));
    }
    void Deserialize(std::istream& is)
    {
        is.read(reinterpret_cast<char*>(&Magic), sizeof(Magic));
        is.read(reinterpret_cast<char*>(&StageCount), sizeof(StageCount));
        is.read(reinterpret_cast<char*>(&HeadersCount), sizeof(HeadersCount));
    }
};
struct PackedSpvEntry
{
    ShaderStage::Enum Stage;
    uint32_t WordCount;

    void Serialize(std::ostream& os) const
    {
        os.write(reinterpret_cast<const char*>(&Stage), sizeof(Stage));
        os.write(reinterpret_cast<const char*>(&WordCount), sizeof(WordCount));
    }
    void Deserialize(std::istream& is)
    {
        is.read(reinterpret_cast<char*>(&Stage), sizeof(Stage));
        is.read(reinterpret_cast<char*>(&WordCount), sizeof(WordCount));
    }
};

uint64_t FileTimeToNs(std::filesystem::file_time_type t)
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(t.time_since_epoch()).count();
}

void SaveCombinedSpv(
    const std::filesystem::path& outPath,
    const FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>>& stages,
    const std::vector<GlslhInfo> glslHeadersInfo)
{
    namespace fs = std::filesystem;
    // Prepare header and entries
    PackedSpvHeader hdr{};
    hdr.Magic = PackedSpvHeader::MagicValue;
    hdr.StageCount = static_cast<uint32_t>(stages.size());
    hdr.HeadersCount = static_cast<uint32_t>(glslHeadersInfo.size());

    std::vector<PackedSpvEntry> entries;
    entries.reserve(stages.size());

    for (const auto& [stage, words] : stages)
    {
        PackedSpvEntry e{};
        e.Stage = stage;
        e.WordCount = static_cast<uint32_t>(words.size());

        entries.push_back(e);
    }

    // Write file
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream os(outPath, std::ios::binary);
    if (!os)
    {
        LNE_ERROR("Failed to open file for writing: {}", outPath.string());
        return;
    }

    // Write header
    hdr.Serialize(os);
    // Write entries
    for (const auto& entry : entries)
        entry.Serialize(os);

    // write number of shader headers
    for (const auto& glslHeader : glslHeadersInfo)
    {
        LNE_INFO("Shader header: {}, Last modified: {}", glslHeader.FullPath.string(), glslHeader.LastModified.time_since_epoch().count());
        fs::path relativeAssetPath = fs::relative(
            fs::weakly_canonical(glslHeader.FullPath),
            fs::weakly_canonical(ApplicationBase::GetAssetsPath())
        );
        uint64_t lastModifiedNs = FileTimeToNs(glslHeader.LastModified);
        uint32_t pathLength = static_cast<uint32_t>(relativeAssetPath.string().length());
        os.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
        os.write(relativeAssetPath.string().c_str(), pathLength);
        os.write(reinterpret_cast<const char*>(&lastModifiedNs), sizeof(lastModifiedNs));

        if (!os)
        {
            LNE_ERROR("Failed to write shader header info to file: {}", outPath.string());
            return;
        }

    }

    // Write SPV words
    for (const auto& [stage, words] : stages)
    {
        if (words.empty())
        {
            LNE_WARN("Skipping empty shader stage: {}", ShaderStage::ToString(stage));
            continue;
        }
        os.write(
            reinterpret_cast<const char*>(words.data()),
            static_cast<uint32_t>(words.size()) * sizeof(uint32_t)
        );
    }

    os.flush();
    if (!os)
    {
        LNE_ERROR("Failed to write SPIR-V data to file: {}", outPath.string());
        return;
    }
}

bool LoadCombinedSpv(const std::filesystem::path& iPath, FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>>& oResult)
{
    namespace fs = std::filesystem;
    std::ifstream is(iPath, std::ios::binary | std::ios::ate);
    if (!is)
    {
        LNE_ERROR("Failed to open SPIR-V file: {}", iPath.string());
        return false;
    }

    const std::streamsize fileSize = is.tellg();
    is.seekg(0, std::ios::beg);

    if (fileSize < sizeof(PackedSpvHeader))
    {
        LNE_ERROR("SPIR-V file is too small: {}", iPath.string());
        return false;
    }

    PackedSpvHeader hdr{};
    hdr.Deserialize(is);
    if (hdr.Magic != PackedSpvHeader::MagicValue)
    {
        LNE_ERROR("Invalid SPIR-V file magic value: {}", iPath.string());
        return false;
    }
    if (hdr.StageCount == 0)
    {
        LNE_ERROR("No shader stages found in SPIR-V file: {}", iPath.string());
        return false;
    }
    oResult.clear();
    oResult.reserve(hdr.StageCount);
    std::vector<PackedSpvEntry> entries(hdr.StageCount);
    for (uint32_t i = 0; i < hdr.StageCount; ++i)
    {
        entries[i].Deserialize(is);
        if (entries[i].WordCount == 0)
        {
            LNE_WARN("Skipping empty shader stage: {}", ShaderStage::ToString(entries[i].Stage));
            continue;
        }
    }
    if (hdr.HeadersCount > 0)
    {
        std::vector<GlslhInfo> glslHeadersInfo(hdr.HeadersCount);
        for (uint32_t i = 0; i < hdr.HeadersCount; ++i)
        {
            uint32_t pathLength = 0;
            is.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
            if (!is || pathLength == 0)
            {
                LNE_ERROR("Failed to read shader header path length: {}", iPath.string());
                return false;
            }
            std::string path(pathLength, '\0');
            is.read(path.data(), pathLength);
            if (!is)
            {
                LNE_ERROR("Failed to read shader header path: {}", iPath.string());
                return false;
            }
            uint64_t lastModifiedNs = 0;
            is.read(reinterpret_cast<char*>(&lastModifiedNs), sizeof(lastModifiedNs));
            if (!is)
            {
                LNE_ERROR("Failed to read shader header last modified time: {}", iPath.string());
                return false;
            }
            fs::path fullPath = fs::path(ApplicationBase::GetAssetsPath()) / fs::weakly_canonical(path);
            if (!fs::exists(fullPath))
            {
                LNE_WARN("Shader header path does not exist: {}", fullPath.string());
                return false;
            }
            auto lastModified = fs::last_write_time(fullPath);
            if (FileTimeToNs(lastModified) != lastModifiedNs)
            {
                LNE_INFO("Header was modified, reloading: {}", fullPath.string());
                return false;
            }
        }
    }
    for (const auto& entry : entries)
    {
        if (entry.WordCount == 0)
            continue; // Skip empty stages
        std::vector<uint32_t> words(entry.WordCount);
        is.read(reinterpret_cast<char*>(words.data()), entry.WordCount * sizeof(uint32_t));
        if (!is)
        {
            LNE_ERROR("Failed to read SPIR-V data for stage: {}", ShaderStage::ToString(entry.Stage));
            return false;
        }
        oResult[entry.Stage] = std::move(words);
    }
    if (is.tellg() != fileSize)
    {
        LNE_ERROR("File size mismatch after reading SPIR-V data: {}", iPath.string());
        return false;
    }
    LNE_INFO("Successfully loaded SPIR-V file: {}", iPath.string());
    return true;
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
    MakePushConstantRange();
}

Shader::~Shader()
{
    ShaderResourceDeletion shaderDeletion {
        .DescriptorSetLayouts = m_CreatedLayouts,
    };
    for (auto& [stage, module] : m_Modules)
        shaderDeletion.ShaderModules.push_back(module);

    ResourceDeletion deletion {
        .Type = ResourceType::eShader,
        .Resource = shaderDeletion
    };
    m_Context->EnqueueResourceDeletion(deletion);
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

    m_Header = ParseHeader(headerSource);

    return { { std::istreambuf_iterator<char>{shaderSourceFile}, {} }, m_Header };
}

Shader::Header Shader::ParseHeader(std::string& headerSource)
{
    static std::string headerPrefix = std::string("//#lne_head ");

    if (headerSource.substr(0, headerPrefix.length()) != headerPrefix)
        LNE_ASSERT(false, "Ill-formed header or header not found");

    Shader::Header header{};
    size_t index = headerPrefix.size();

    while (index < headerSource.size())
    {
        if (headerSource[index] == '[')
        {
            index++; // Entering an individual stage declaration, e.g., "[Vt main]"
            std::string token;
            // Read the token (e.g., "Vt" or "Fg")
            while (index < headerSource.size() && !std::isspace(headerSource[index]))
            {
                token.push_back(headerSource[index]);
                index++;
            }
            // Skip the whitespace between the token and the value
            while (index < headerSource.size() && std::isspace(headerSource[index]))
            {
                index++;
            }
            std::string value;
            while (index < headerSource.size() && headerSource[index] != ']')
            {
                value.push_back(headerSource[index]);
                index++;
            }
            LNE_ASSERT(index < headerSource.size() && headerSource[index] == ']',
                "Missing closing bracket in stage declaration");
            index++; // skip the closing ']'

            auto stage = MapShaderToken(token);
            if (stage != ShaderStage::eUnknown)
            {
                header.StageHeaders[stage] = ShaderHeaderInfo{ value };
                continue;
            }
            if (token == "Rp")
            {
                header.RenderPass = value;
                header.RenderPassHash = GlobalUtils::HashU64(std::hash<std::string>{}(value));
                continue;
            }
            if (token == "Tp")
            {
                header.ShaderDomain = MapMaterialTypeToken(value);
                continue;
            }
            LNE_WARN("Unknown token in shader header: {}", token);
        }
        else if (headerSource[index] == ']')
        {
            index++;
            if (index < headerSource.size() && headerSource[index] == ']')
            {
                index++; // End of nested stage declarations
                break;
            }
        }
        else
        {
            index++;
        }
    }

    return header;
}

FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> Shader::CompileToSpirv(const std::string& sourceCode, Shader::Header header)
{
    // Check if the shader has been compiled before && if the source code hasn't changed
    std::filesystem::path cachePath = ApplicationBase::GetRenderer().GetShaderCachePath() / (m_Name + ".pspv"); // Packed SPIR-V file

    if (std::filesystem::exists(cachePath))
    {
        // check m_FilePath and cachePath for modification time
        auto fileTime = std::filesystem::last_write_time(m_FilePath);
        auto cacheTime = std::filesystem::last_write_time(cachePath);
        if (fileTime <= cacheTime)
        {
            LNE_INFO("Using cached SPIR-V for shader: {}", m_Name);
            FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> cachedSpirv;
            bool success = LoadCombinedSpv(cachePath, cachedSpirv);
            if (success && !cachedSpirv.empty())
                return cachedSpirv;
        }
        else
            LNE_INFO("Shader source changed, recompiling: {}", m_Name);
    }

    // Compile shaders
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    constexpr bool optimize = false;
    options.SetOptimizationLevel(optimize ? shaderc_optimization_level_performance : shaderc_optimization_level_zero);
    options.SetWarningsAsErrors();
    std::vector<GlslhInfo> shaderHeaders;
    std::function<void(GlslhInfo&&)> shaderHeaderCallback = [&shaderHeaders](GlslhInfo&& headerInfo)
    {
        shaderHeaders.emplace_back(std::move(headerInfo));
    };
    options.SetIncluder(std::make_unique<ShaderFileIncluder>(
        ApplicationBase::GetRenderer().GetShaderIncludeDirs(),
        shaderHeaderCallback
    ));
    FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> spirvCode;
    std::vector<shaderc::CompileOptions> optionsForShaders(header.StageHeaders.size(), options);
    uint32_t optionsIndex = 0;

    for (auto& [stage, headerInfo] : header.StageHeaders)
    {
        auto& options = optionsForShaders.back();
        options.AddMacroDefinition(ShaderStageToDefine(stage), "1");

        shaderc_shader_kind shaderKind = ShaderStageToShaderc(stage);
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
            sourceCode.c_str(),
            sourceCode.size(),
            shaderKind,
            m_FilePath.c_str(),
            headerInfo.EntryPoint.c_str(),
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
    // Save the compiled SPIR-V code to a file
    SaveCombinedSpv(cachePath, spirvCode, shaderHeaders);

    return spirvCode;
}

void Shader::ReflectOnSpirv(FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> spirvCode)
{
    MatTypeInfo matTypeInfo{};
    bool isUnknownMatType = (m_Header.ShaderDomain == ShaderDomain::eUnknown);
    if (isUnknownMatType == false)
        matTypeInfo = MatTypeInfos[m_Header.ShaderDomain];
    for (auto& [stage, code] : spirvCode)
    {
        LNE_INFO("Stage: {}", ShaderStageToDefine(stage));
        spirv_cross::Compiler compiler(code);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        // =============== UNIFORM BUFFERS ===============
        for (const auto& res : resources.uniform_buffers)
        {
            uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
            uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
            spirv_cross::SPIRType type = compiler.get_type(res.base_type_id);
            uint32_t bufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            if (!m_ReflectedData.DescriptorSets.contains(set))
                m_ReflectedData.DescriptorSets[set] = DescriptorSet{ .SetIndex = set };

            auto& setRef = m_ReflectedData.DescriptorSets[set];
            auto it = setRef.UniformBuffers.find(res.name);
            if (it != setRef.UniformBuffers.end())
            {
                it->second.Stages = it->second.Stages | ShaderStageToVk(stage);
            }
            else
            {
                setRef.UniformBuffers[res.name] = {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .Size = bufferSize,
                    .Stages = ShaderStageToVk(stage)
                };
            }


            if (!(isUnknownMatType == false &&
                (set != matTypeInfo.SetIndices[ShaderSetIndexType::eMaterial] &&
                 set != matTypeInfo.SetIndices[ShaderSetIndexType::eVertex])))
                LNE_INFO("    UBO Name: {}, Set: {}, Binding: {}, Size: {}", res.name, set, binding, bufferSize);

            for (uint32_t i = 0; i < type.member_types.size(); ++i)
            {
                std::string memberName = compiler.get_member_name(res.base_type_id, i);
                uint32_t offset = compiler.get_member_decoration(res.base_type_id, i, spv::DecorationOffset);
                uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(type, i));
                spirv_cross::SPIRType memberType = compiler.get_type(type.member_types[i]);

                if (!(isUnknownMatType == false &&
                      (set != matTypeInfo.SetIndices[ShaderSetIndexType::eMaterial] &&
                       set != matTypeInfo.SetIndices[ShaderSetIndexType::eVertex])))
                    LNE_INFO("        Member: {}, Offset: {}, Size: {}, Type: {}",
                         memberName, offset, size, ShaderElementType::ToString(SpirvTypeToUniformElementType(memberType)));

                m_ReflectedData.UniformElements[memberName] = {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .Offset = offset,
                    .Size = size,
                    .Type = SpirvTypeToUniformElementType(memberType)
                };
            }
        }

        // =============== STORAGE BUFFERS ===============
        for (const auto& res : resources.storage_buffers)
        {
            uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
            uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
            spirv_cross::SPIRType type = compiler.get_type(res.base_type_id);
            uint32_t bufferSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            if (!m_ReflectedData.DescriptorSets.contains(set))
                m_ReflectedData.DescriptorSets[set] = DescriptorSet{ .SetIndex = set };

            auto& setRef = m_ReflectedData.DescriptorSets[set];
            auto it = setRef.StorageBuffers.find(res.name);
            if (it != setRef.StorageBuffers.end())
            {
                it->second.Stages = it->second.Stages | ShaderStageToVk(stage);
            }
            else
            {
                setRef.StorageBuffers[res.name] = {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .Size = bufferSize,
                    .Stages = ShaderStageToVk(stage)
                };
            }

            if (!(isUnknownMatType == false &&
                  (set != matTypeInfo.SetIndices[ShaderSetIndexType::eMaterial] &&
                   set != matTypeInfo.SetIndices[ShaderSetIndexType::eVertex])))
                LNE_INFO("    SSBO Name: {}, Set: {}, Binding: {}, Declared Size (w/o runtime part): {}",
                     res.name, set, binding, bufferSize);

            setRef.StorageBuffers[res.name].Size = ReflectSSBOStructMembers(compiler, res.base_type_id, res.name, set, binding);
        }

        // =============== PUSH CONSTANTS ===============
        for (const auto& res : resources.push_constant_buffers)
        {
            spirv_cross::SPIRType type = compiler.get_type(res.base_type_id);
            const uint32_t declaredSize = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            // SPIRV-Cross can report per-stage active ranges for this push block.
            const auto ranges = compiler.get_active_buffer_ranges(res.id);

            // Compute the minimal active [minOffset, maxEnd) for THIS stage.
            uint32_t stageMin = UINT32_MAX;
            uint32_t stageMax = 0;
            for (const auto& r : ranges)
            {
                stageMin = std::min(stageMin, (uint32_t)r.offset);
                stageMax = std::max(stageMax, (uint32_t)r.offset + (uint32_t)r.range);
            }
            if (ranges.empty())
            {
                // Fallback: if active ranges not reported, use the declared size from 0.
                stageMin = 0;
                stageMax = declaredSize;
            }

            auto& block = m_ReflectedData.PushConstants[res.name];
            if (block.Size == 0 && block.Stages == vk::ShaderStageFlags{})
            {
                // First time we see it
                block.Offset = stageMin;
                block.Size = (stageMax - stageMin);
            }
            else
            {
                // Merge across stages
                const uint32_t curEnd = block.Offset + block.Size;
                const uint32_t newMin = std::min(block.Offset, stageMin);
                const uint32_t newMax = std::max(curEnd, stageMax);
                block.Offset = newMin;
                block.Size = newMax - newMin;
            }

            block.Stages |= ShaderStageToVk(stage);

            LNE_INFO("    PUSH Name: {}, Stage: {}, DeclaredSize: {}, ActiveRange: [{}..{}), Merged: offset {}, size {}",
                     res.name, ShaderStageToDefine(stage), declaredSize, stageMin, stageMax, block.Offset, block.Size);

            // Reflect members of the push-constant struct
            for (uint32_t i = 0; i < type.member_types.size(); ++i)
            {
                const std::string memberName = compiler.get_member_name(res.base_type_id, i);
                const uint32_t offset = compiler.get_member_decoration(res.base_type_id, i, spv::DecorationOffset);
                const uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(type, i));
                const spirv_cross::SPIRType memberType = compiler.get_type(type.member_types[i]);

                LNE_INFO("        PC Member: {} | Offset {}, Size {}, Type {}",
                         memberName, offset, size, ShaderElementType::ToString(SpirvTypeToUniformElementType(memberType)));

                block.Members[memberName] = {
                    .Offset = offset,
                    .Size = size,
                    .Type = SpirvTypeToUniformElementType(memberType)
                };
            }
        }
    }
}

uint32_t Shader::ReflectSSBOStructMembers(spirv_cross::Compiler& compiler, uint32_t struct_type_id, const std::string& prefix, uint32_t set, uint32_t binding)
{
    MatTypeInfo matTypeInfo{};
    bool isUnknownMatType = (m_Header.ShaderDomain == ShaderDomain::eUnknown);
    if (isUnknownMatType == false)
        matTypeInfo = MatTypeInfos[m_Header.ShaderDomain];

    auto LogMember = [&](const std::string& qname, uint32_t set, uint32_t binding,
                         uint32_t offset, uint32_t size, const spirv_cross::SPIRType& memberType)
        {
            if (isUnknownMatType == false &&
                (set != matTypeInfo.SetIndices[ShaderSetIndexType::eMaterial] &&
                 set != matTypeInfo.SetIndices[ShaderSetIndexType::eVertex]))
                return;
            LNE_INFO("        Member: {} | Set {}, Binding {}, Offset {}, Size {}, Type {}",
                     qname, set, binding, offset, size, ShaderElementType::ToString(SpirvTypeToUniformElementType(memberType)));
        };

    const spirv_cross::SPIRType& st = compiler.get_type(struct_type_id);
    uint32_t fullStride{};
    for (uint32_t i = 0; i < st.member_types.size(); ++i)
    {
        const std::string memberName = compiler.get_member_name(struct_type_id, i);
        const std::string qname = prefix.empty() ? memberName : (prefix + "." + memberName);

        const uint32_t offset = compiler.get_member_decoration(struct_type_id, i, spv::DecorationOffset);
        const uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_member_size(st, i));
        const spirv_cross::SPIRType& memberType = compiler.get_type(st.member_types[i]);

        LogMember(qname, set, binding, offset, size, memberType);

        auto TryReflectArrayElementStruct = [&](const std::string& arrayQname)
            {
                const uint32_t arrayStride =
                    static_cast<uint32_t>(compiler.type_struct_member_array_stride(st, i));

                uint32_t elem_type_id = memberType.parent_type;
                if (elem_type_id == 0)
                    return arrayStride;

                const spirv_cross::SPIRType& elemType = compiler.get_type(elem_type_id);
                if (elemType.basetype != spirv_cross::SPIRType::Struct)
                    return arrayStride;

                const uint32_t elemSize =
                    static_cast<uint32_t>(compiler.get_declared_struct_size(elemType));

                LNE_INFO("        Array: {} | arrayStride {}, elementSize {}",
                         arrayQname, arrayStride, elemSize);

                for (uint32_t j = 0; j < elemType.member_types.size(); ++j)
                {
                    const std::string elemMemberName = compiler.get_member_name(elem_type_id, j);

                    const uint32_t elemOffset =
                        compiler.get_member_decoration(elem_type_id, j, spv::DecorationOffset);
                    const uint32_t elemMemberSize =
                        static_cast<uint32_t>(compiler.get_declared_struct_member_size(elemType, j));
                    const spirv_cross::SPIRType& elemMemberType =
                        compiler.get_type(elemType.member_types[j]);

                    LogMember(elemMemberName, set, binding, elemOffset, elemMemberSize, elemMemberType);

                    m_ReflectedData.StorageElements[elemMemberName] = {
                        .SetIndex = set,
                        .BindingIndex = binding,
                        .Offset = elemOffset,
                        .Size = elemMemberSize,
                        .Type = SpirvTypeToUniformElementType(elemMemberType),
                        .ArrayStride = arrayStride
                    };

                    m_ReflectedData.StorageArrays[arrayQname] = {
                    .SetIndex = set,
                    .BindingIndex = binding,
                    .ArrayStride = arrayStride,
                    .ElementSize = elemSize
                    };
                }
                return arrayStride;
            };

        if (!memberType.array.empty())
        {
            const std::string arrayQName = qname + "[]";
            fullStride += TryReflectArrayElementStruct(arrayQName);
        }
        else if (memberType.basetype == spirv_cross::SPIRType::Struct)
        {
            ReflectSSBOStructMembers(compiler, st.member_types[i], qname, set, binding);
        }

        // Finally, if you want to record leaf scalars/vectors/matrices too:
        if (memberType.basetype != spirv_cross::SPIRType::Struct && memberType.array.empty())
        {
            m_ReflectedData.StorageElements[qname] = {
                .SetIndex = set,
                .BindingIndex = binding,
                .Offset = offset,
                .Size = size,
                .Type = SpirvTypeToUniformElementType(memberType),
                .ArrayStride = 0u
            };
        }
    }
    return fullStride;
}

FlatHashMap<ShaderStage::Enum, vk::ShaderModule> Shader::CreateModules(FlatHashMap<ShaderStage::Enum, std::vector<uint32_t>> spirvCode)
{
    vk::ShaderModuleCreateInfo createInfo;
    FlatHashMap<ShaderStage::Enum, vk::ShaderModule> modules;

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
    using StageFlags = vk::ShaderStageFlagBits;
    MatTypeInfo matTypeInfo{};
    if (m_Header.ShaderDomain != ShaderDomain::eUnknown)
        matTypeInfo = MatTypeInfos[m_Header.ShaderDomain];

    auto setStageIfNeeded = [&matTypeInfo, this](int32_t setIndex, vk::ShaderStageFlags& stages)
    {
        if (m_Header.ShaderDomain == ShaderDomain::eUnknown)
            return;
        if (setIndex == matTypeInfo.SetIndices[ShaderSetIndexType::eGlobal])
        {
            stages = StageFlags::eAll;
            return;
        }
    };
    for (auto&[setIndex, set] : m_ReflectedData.DescriptorSets)
    {
        if (setIndex == matTypeInfo.SetIndices[ShaderSetIndexType::eVertex])
        {
            if (m_Header.ShaderDomain == ShaderDomain::eMeshlet)
                m_DescriptorSetLayouts[layoutIndex] = m_Context->GetStorageOnlyDescriptorSetLayout(4);
            else
                m_DescriptorSetLayouts[layoutIndex] = m_Context->GetStorageOnlyDescriptorSetLayout(2);
            layoutIndex++;
            continue;
        }
        else if (setIndex == matTypeInfo.SetIndices[ShaderSetIndexType::eTransform])
        {
            m_DescriptorSetLayouts[layoutIndex] = m_Context->GetStorageOnlyDescriptorSetLayout(1);
            layoutIndex++;
            continue;
        }

        vk::DescriptorSetLayoutCreateInfo descSetLayoutCI{};
        descSetLayoutCI.setBindingCount((uint32_t)set.UniformBuffers.size() + (uint32_t)set.StorageBuffers.size());
        std::vector<vk::DescriptorSetLayoutBinding> bindings{};
        bindings.reserve(descSetLayoutCI.bindingCount);
        for (auto& [name, buffer] : set.UniformBuffers)
        {
            auto stages = buffer.Stages;
            setStageIfNeeded(buffer.SetIndex, stages);

            bindings.emplace_back(vk::DescriptorSetLayoutBinding(buffer.BindingIndex, vk::DescriptorType::eUniformBuffer, 1, stages));
        }
        for (auto& [name, buffer] : set.StorageBuffers)
        {
            auto stages = StageFlags::eAll;

            bindings.emplace_back(vk::DescriptorSetLayoutBinding(buffer.BindingIndex, vk::DescriptorType::eStorageBuffer, 1, stages));
        }
        descSetLayoutCI.setBindings(bindings);
        m_DescriptorSetLayouts[layoutIndex] = m_Context->GetDevice().createDescriptorSetLayout(descSetLayoutCI);
        m_CreatedLayouts.emplace_back(m_DescriptorSetLayouts[layoutIndex]);
        m_Context->SetVkObjectName(m_DescriptorSetLayouts[layoutIndex], std::format("DescSetLayout {}, set: {}", m_Name, setIndex));
        layoutIndex++;
    }
}

void Shader::MakePushConstantRange()
{
    m_PushConstantRanges.reserve(m_ReflectedData.PushConstants.size());
    for (const auto& [name, pc] : m_ReflectedData.PushConstants)
    {
        if (pc.Size == 0)
            continue;

        vk::PushConstantRange r{};
        r.stageFlags = pc.Stages;
        r.offset = pc.Offset;
        r.size = pc.Size;
        m_PushConstantRanges.push_back(r);
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
    case ShaderStage::eTask: return vk::ShaderStageFlagBits::eTaskEXT;
    case ShaderStage::eMesh: return vk::ShaderStageFlagBits::eMeshEXT;
    default: return vk::ShaderStageFlagBits::eVertex;
    }
}
