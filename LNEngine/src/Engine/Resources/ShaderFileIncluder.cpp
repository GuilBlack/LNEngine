
#include "ShaderFileIncluder.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Log.h"

#ifdef LNE_PLATFORM_WINDOWS
#define strdup _strdup
#endif

namespace lne
{
static bool ReadFileToString(const std::filesystem::path& path, std::string& out)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;
    file.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0);
    out.resize(size);
    file.read(out.data(), size);
    return true;
}

shaderc_include_result* ShaderFileIncluder::GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source, size_t include_depth)
{
    std::filesystem::path resolved;
    bool found = false;

    std::filesystem::path request(requested_source);

    if (type == shaderc_include_type_relative)
    {
        if (requesting_source && std::strlen(requesting_source) > 0)
        {
            std::filesystem::path parent = std::filesystem::path(requesting_source).parent_path();
            std::filesystem::path candidate = parent / request;
            if (std::filesystem::exists(candidate))
            {
                resolved = std::filesystem::canonical(candidate);
                found = true;
            }
        }
    }

    if (!found)
    {
        for (const auto& dir : m_IncludeDirs)
        {
            std::filesystem::path candidate = dir / request;
            if (std::filesystem::exists(candidate))
            {
                resolved = std::filesystem::canonical(candidate);
                found = true;
                break;
            }
        }
    }

    std::string content;
    std::string source_name_str;
    if (found)
    {
        if (!ReadFileToString(resolved, content))
        {
            source_name_str = requested_source;
            std::string error_msg = std::string("Failed to read include file: ") + resolved.string();
            LNE_ERROR(std::format("{}: {}", source_name_str.c_str(), error_msg.c_str()));
            return MakeErrorResult(source_name_str.c_str(), error_msg.c_str());
        }
        source_name_str = resolved.string();
    }
    else
    {
        source_name_str = requested_source;
        std::string error_msg = std::string("Include file not found: ") + requested_source;
        return MakeErrorResult(source_name_str.c_str(), error_msg.c_str());
    }

    return MakeResult(source_name_str.c_str(), content.c_str(), content.size());
}

void ShaderFileIncluder::ReleaseInclude(shaderc_include_result* data)
{
    if (!data) 
        return;
    free((void*)data->content);
    free((void*)data->source_name);
    delete data;
}

shaderc_include_result* ShaderFileIncluder::MakeResult(const char* source_name, const char* content, size_t content_length)
{
    auto* result = new shaderc_include_result();
    result->source_name = strdup(source_name);
    result->source_name_length = std::strlen(result->source_name);
    result->content = strdup(content);
    result->content_length = content_length;
    result->user_data = nullptr;
    return result;
}

shaderc_include_result* ShaderFileIncluder::MakeErrorResult(const char* source_name, const char* error_msg)
{
    return MakeResult(source_name, error_msg, std::strlen(error_msg));
}
}
