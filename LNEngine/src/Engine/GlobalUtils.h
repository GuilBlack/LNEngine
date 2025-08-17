#pragma once

namespace lne 
{
class GlobalUtils
{
public:
    static void PrintLine(const std::string& msg);
    static std::size_t hash_u64(uint64_t value) noexcept;
};
}