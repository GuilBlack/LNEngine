#pragma once
#include <shaderc/shaderc.hpp>

namespace lne
{
struct GlslhInfo
{
    std::filesystem::path FullPath;
    std::filesystem::file_time_type LastModified;
};

class ShaderFileIncluder : public shaderc::CompileOptions::IncluderInterface
{
public:
    explicit ShaderFileIncluder(std::vector<std::filesystem::path> include_dirs, std::function<void(GlslhInfo&&)>& shaderInfoCallback)
        : m_IncludeDirs(std::move(include_dirs)), m_ShaderInfoCallback(shaderInfoCallback)
    {}

    virtual ~ShaderFileIncluder() = default;

    virtual shaderc_include_result* GetInclude(const char* requested_source,
        shaderc_include_type type,
        const char* requesting_source,
        size_t include_depth) override;

    virtual void ReleaseInclude(shaderc_include_result* data) override;

private:
    std::vector<std::filesystem::path> m_IncludeDirs;
    std::function<void(GlslhInfo&&)> m_ShaderInfoCallback;

private:
    static shaderc_include_result* MakeResult(const char* source_name,
        const char* content,
        size_t content_length);
    static shaderc_include_result* MakeErrorResult(const char* source_name, const char* error_msg);
};
}
