#include "IRenderPass.h"

namespace lne
{
void lne::IDrawStaticMeshes::AddStaticMeshDrawCommand(const StaticMeshHash& hash, SafePtr<StaticMesh> mesh, uint32_t subMeshIndex)
{
    const SubMesh& submesh = mesh->GetSubMeshes()[hash.SubMeshIndex];
    SafePtr material = mesh->GetMaterial(submesh.MaterialIndex);
    if (material->IsTransparent())
        return;
    auto& drawCommands = m_DrawCommands[hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}
}
