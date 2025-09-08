#pragma once
#include "Engine/Graphics/Structs.h"
#include "../vendor/BOOST/include/Unordered/boost_unordered.hpp"

namespace boost
{
// based on boost's hash_combine
template <>
struct hash<lne::StaticMeshHash>
{
    std::size_t operator()(const lne::StaticMeshHash& k) const noexcept
    {
        std::size_t seed{ 0 };
        hash_combine(seed, k.MeshAddress);
        hash_combine(seed, k.SubMeshIndex);
        return seed;
    }
};
}
