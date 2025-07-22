#pragma once
#include "StorageBuffer.h"
#include "Structs.h"
#include <unordered_map>
#include "Engine/Graphics/Texture.h"
#include "Engine/Graphics/Material.h"
#include "Engine/Core/Utils/_Defines.h"


namespace lne
{
struct Vertex
{
    glm::vec3 Position;
    glm::vec2 TexCoord;
    glm::vec3 Normal;
    glm::vec4 Tangent;
};

struct Geometry
{
    SafePtr<StorageBuffer>  VertexGPUBuffer{};
    SafePtr<StorageBuffer>  IndexGPUBuffer{};
    void*                   Vertices{};
    void*                   Indices{};

    uint32_t VertexCount{};
    uint32_t IndexCount{};

    ~Geometry()
    {
        delete[] Vertices;
        delete[] Indices;
    }

    static Geometry GenerateCube(uint32_t tesselationLevel);

    static Geometry GenerateUVSphere(float radius = 1.f, uint32_t nLatitude = 32, uint32_t nLongitude = 32);

    Geometry() = default;
    Geometry(Geometry&& other) noexcept
        : VertexGPUBuffer(std::move(other.VertexGPUBuffer)),
        IndexGPUBuffer(std::move(other.IndexGPUBuffer)),
        Vertices(other.Vertices),
        Indices(other.Indices),
        VertexCount(other.VertexCount),
        IndexCount(other.IndexCount)
    {
        other.Vertices = nullptr;
        other.Indices = nullptr;
        other.IndexCount = 0;
        other.VertexCount = 0;
    }

private:
    Geometry(const Geometry&) = delete;
    Geometry& operator=(const Geometry&) = delete;
    Geometry& operator=(Geometry&) = delete;
    Geometry operator=(Geometry) = delete;
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

class StaticMesh : public RefCountBase
{
public:
    // TODO: probably make a mesh importer class or something
    StaticMesh(std::filesystem::path path, SafePtr<class GfxPipeline> pipeline, SafePtr<class GfxPipeline> transparentPipeline);
    StaticMesh(Geometry&& geometry,
        SafePtr<class Material> material, std::vector<SafePtr<class Texture>> textures,
        SafePtr<class GfxPipeline> pipeline);

    std::vector<SubMesh>& GetSubMeshes() { return m_SubMeshes; }
    const Geometry& GetGeometry() const { return m_Geometry; }
    SafePtr<class GfxPipeline> GetPipeline() { return m_Pipeline; }
    SafePtr<class Material> GetMaterial(uint32_t index) { return m_Materials[index]; }

private:
    std::filesystem::path m_Path{};
    std::vector<SubMesh> m_SubMeshes{};

    Geometry m_Geometry{};
    uint32_t m_TotalVertexCount{};
    uint32_t m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<class Material>> m_Materials{};
    SafePtr<class GfxPipeline> m_Pipeline{};
    SafePtr<class GfxPipeline> m_TransparentPipeline{};
    std::vector<SafePtr<class Texture>> m_Textures{};
private:
    void InitSubmeshes(const struct aiScene* scene);
    void LoadData(const struct aiScene* scene);
    void LoadMaterials(const struct aiScene* scene);
    void TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);
};

}
