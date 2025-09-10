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

template<>
struct hash<lne::PipelineHandle>
{
    std::size_t operator()(const lne::PipelineHandle& p) const
    {
        size_t seed = 0;
        hash_combine(seed, p.H1);
        hash_combine(seed, p.H2);
        return seed;
    }
};

template<>
struct hash<lne::MaterialPipelineHash>
{
    std::size_t operator()(const lne::MaterialPipelineHash& k) const
    {
        size_t seed = 0;
        hash_combine(seed, k.PassId);
        hash_combine(seed, k.FrameGraphHash);
        return seed;
    }
};
}
