#include "IRenderPass.h"

namespace lne
{
void IDrawStaticMeshes::AddStaticMeshDrawCommand(StaticMeshHash hash, SafePtr<StaticMesh> mesh, uint32_t subMeshIndex)
{
    auto& drawCommands = m_DrawCommands[hash];
    drawCommands.Mesh = mesh;
    drawCommands.SubMeshIndex = subMeshIndex;
    drawCommands.InstanceCount++;
}
}
