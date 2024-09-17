#pragma once
#include "StorageBuffer.h"
#include "Structs.h"

namespace lne
{
struct Geometry
{
    SafePtr<StorageBuffer> VertexGPUBuffer;
    SafePtr<StorageBuffer> IndexGPUBuffer;

    uint32_t VertexCount;
    uint32_t IndexCount;
};

struct SubMesh
{
    uint32_t BaseVertex;
    uint32_t BaseIndex;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t MaterialIndex;
    AABB BoundingBox;
    std::string Name;

    glm::mat4 WorldTransform = glm::mat4(1.0f);
};

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
};

class StaticMesh : public RefCountBase
{
public:
    StaticMesh(std::filesystem::path path, SafePtr<class GfxPipeline> pipeline);

    std::vector<SubMesh>& GetSubMeshes() { return m_SubMeshes; }
    const Geometry& GetGeometry() const { return m_Geometry; }
    SafePtr<class GfxPipeline> GetPipeline() { return m_Pipeline; }
    SafePtr<class Material> GetMaterial(uint32_t index) { return m_Materials[index]; }

private:
    std::filesystem::path m_Path{};
    std::vector<SubMesh> m_SubMeshes{};

    Geometry m_Geometry{};
    std::vector<Vertex> m_Vertices{};
    std::vector<uint32_t> m_Indices{};
    uint32_t m_TotalVertexCount{};
    uint32_t m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<class Material>> m_Materials{};
    SafePtr<class GfxPipeline> m_Pipeline{};
    std::vector<SafePtr<class Texture>> m_Textures{};
private:
    void InitSubmeshes(const struct aiScene* scene);
    void LoadData(const struct aiScene* scene);
    void LoadMaterials(const struct aiScene* scene);
    void TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);
};

}
