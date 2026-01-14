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

    void                                SetEnvironmentMap(std::string_view pathToEnvMap);
    void                                SetSunLightDirection(const glm::vec3& direction) { m_Environment->SunLight.Direction = direction; }
    void                                SetSunLightColor(const glm::vec3& color) { m_Environment->SunLight.Color = color; }
    void                                SetSunLightIntensity(float intensity) { m_Environment->SunLight.Intensity = intensity; }
    void                                SetIsSunEnabled(bool isEnabled) { m_Environment->IsSunEnabled = isEnabled; }

    void                                SetAmbientLight(float ambientLight) { m_Environment->AmbientLight = ambientLight; }

    SafePtr<WorldEnvironment>           GetEnvironment() const { return m_Environment; }

    const TransformBuffer&              GetTransformBuffer(uint32_t frameIndex) const  { return m_TransformBuffers[frameIndex]; }
    const SubMeshTransformArray&        GetTransforms(uint32_t frameIndex, StaticMeshHash hash) { return m_Transforms[frameIndex][hash]; }

    SafePtr<StandaloneStorageBuffer>    GetLightBufferGPU(uint32_t frameIndex) const { return m_LightBuffersGPU[frameIndex]; }
    uint32_t                            GetNumLights(uint32_t frameIndex) const { return m_NumLights[frameIndex]; }

    void                                BeginScene(class Entity& cameraEntity);
    void                                Render(class EntityRegistry& registry);
    void                                EndFrame();

private:
    friend class Renderer;
    SafePtr<FrameGraph>                             m_FrameGraph{};
    SafePtr<WorldEnvironment>                       m_Environment{};

    std::vector<FlatHashMap<StaticMeshHash, SubMeshTransformArray>> m_Transforms{};
    std::vector<TransformBuffer>                    m_TransformBuffers{};
    WorldData                                       m_GlobalData{};
    std::vector<SafePtr<UniformBuffer>>             m_WorldGlobalUniforms{};
    std::vector<SafePtr<StandaloneStorageBuffer>>   m_LightBuffersGPU{};
    std::vector<std::vector<LightGPUData>>          m_LightsCPU{};
    std::vector<uint32_t>                           m_NumLights{ 0 };
};
}
