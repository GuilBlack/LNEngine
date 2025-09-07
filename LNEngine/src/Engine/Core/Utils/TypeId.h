#pragma once

namespace lne
{
using TypeId = size_t;
template<typename T>
struct TypeIdHelper
{
    static TypeId Get()
    {
        static const char type_id_var;
        return (TypeId)&type_id_var;
    }
};
}
