#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/StructsHashes.h"

namespace lne
{
class IDrawStaticMeshesAdder
{
public:
    struct DrawCommand
    {
        SafePtr<class StaticMesh>   Mesh;
        u32                         SubMeshIndex;
        u32                         InstanceCount;
    };
public:
    IDrawStaticMeshesAdder();
    virtual ~IDrawStaticMeshesAdder() = default;
    virtual void AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<class StaticMesh> mesh, u32 subMeshIndex, u32 instanceCount) = 0;
    void ClearDrawCommands();
protected:
    std::vector<FlatHashMap<StaticMeshHash, DrawCommand>> m_DrawCommands;
};
}
