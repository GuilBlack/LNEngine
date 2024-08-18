#pragma once
#include "Engine/Core/SafePtr.h"
#include "StorageBuffer.h"

namespace lne
{
struct Geometry
{
    SafePtr<StorageBuffer> VertexGPUBuffer;
    SafePtr<StorageBuffer> IndexGPUBuffer;

    uint64_t VertexCount;
    uint64_t IndexCount;
};
}
