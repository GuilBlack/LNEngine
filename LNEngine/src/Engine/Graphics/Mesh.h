#pragma once
#include "Engine/Core/SafePtr.h"
#include "StorageBuffer.h"

namespace lne
{
struct Geometry
{
    SafePtr<StorageBuffer> VertexGPUBuffer;
    SafePtr<StorageBuffer> IndexGPUBuffer;

    uint32_t VertexCount;
    uint32_t IndexCount;
};
}
