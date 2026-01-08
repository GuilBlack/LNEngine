#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/StructsHashes.h"
#include "Engine/Graphics/Resources/StorageBuffer.h"
#include "Engine/Core/DataStructures/CircularBuffer.h"
#include "Engine/Graphics/WorldEnvironment.h"
#include "Engine/Graphics/GlobalGfxData.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

class FrameGraph;

namespace lne
{
class StaticMesh;
struct SubMeshTransformArray
{
    SafePtr<StaticMesh>         Mesh;
    std::vector<glm::mat4>      Transforms;
    uint32_t                    Offset;
};

struct TransformBuffer
{
    SafePtr<StandaloneStorageBuffer> Buffer;
    glm::mat4* Data;
};

class WorldRenderer : public RefCountBase
{
public:
    WorldRenderer(const SafePtr<FrameGraph>& frameGraph);
    ~WorldRenderer();

    void                            SetEnvironmentMap(std::string_view pathToEnvMap);
    void                            SetSunLightDirection(const glm::vec3& direction) { m_Environment->SunDirection = direction; }
    void                            SetAmbientLight(float ambientLight) { m_Environment->AmbientLight = ambientLight; }
    SafePtr<WorldEnvironment>       GetEnvironment() const { return m_Environment; }

    TransformBuffer&                GetTransformBuffer(uint32_t frameIndex) { return m_TransformBuffers[frameIndex]; }
    SubMeshTransformArray&          GetTransforms(uint32_t frameIndex, StaticMeshHash hash) { return m_Transfroms[frameIndex][hash]; }

    void                            BeginScene(class Entity& cameraEntity);
    void                            Render(class EntityRegistry& registry);
    void                            EndFrame();

private:
    friend class Renderer;
    SafePtr<FrameGraph>                         m_FrameGraph{};
    SafePtr<WorldEnvironment>                   m_Environment{};

    std::vector<FlatHashMap<StaticMeshHash, SubMeshTransformArray>> m_Transfroms{};
    std::vector<TransformBuffer>                m_TransformBuffers{};
    WorldData                                   m_GlobalData{};
    std::vector<SafePtr<UniformBuffer>>         m_WorldGlobalUniforms{};

private:
};
}
