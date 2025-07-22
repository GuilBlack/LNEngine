#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Graphics/StorageBuffer.h"
#include "Engine/Core/DataStructures/CircularBuffer.h"

class FrameGraph;


namespace lne
{
struct SubMeshTransformArray
{
    std::vector<glm::mat4>  Transforms;
    uint32_t                Offset;
};

struct TransformBuffer
{
    SafePtr<class StorageBuffer> Buffer;
    glm::mat4* Data;
};

class WorldRenderer : public RefCountBase
{
public:
    WorldRenderer(const SafePtr<FrameGraph>& frameGraph);
    ~WorldRenderer();

    void SetEnvironmentMap(std::string_view pathToEnvMap);

    TransformBuffer& GetTransformBuffer(uint32_t frameIndex) { return m_TransformBuffers[frameIndex]; }
    SubMeshTransformArray& GetTransforms(StaticMeshHash hash) { return m_Transfroms[hash]; }

    void BeginFrame();
    void Render(class EntityRegistry& registry);
    void EndFrame();

private:
    SafePtr<FrameGraph> m_FrameGraph{};
    SafePtr<class WorldEnvironment> m_Environment{};

    std::unordered_map<StaticMeshHash, SubMeshTransformArray> m_Transfroms{};
    std::vector<TransformBuffer> m_TransformBuffers{};
};
}
