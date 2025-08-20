#pragma once
#include "Structs.h"
#include <unordered_map>
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Graphics/StorageBuffer.h"
#include "Engine/Graphics/Material.h"


namespace lne
{
class GfxContext;
class StorageBuffer;

struct Vertex
{
    glm::vec3 Position;
    glm::vec2 TexCoord;
    glm::vec3 Normal;
    glm::vec4 Tangent;
};

class Geometry
{
public:
    Geometry(
        GfxContext* ctx,
        SafePtr<StorageBuffer> vertexGPUBuffer, SafePtr<StorageBuffer> indexGPUBuffer,
        void* vertices, void* indices,
        uint32_t vertexCount, uint32_t indexCount);

    ~Geometry();

    Geometry(Geometry&& other) noexcept;
    Geometry& operator=(Geometry&& other) noexcept;

    [[nodiscard]] uint32_t GetVertexCount() const { return VertexCount; }
    [[nodiscard]] uint32_t GetIndexCount() const { return IndexCount; }
    [[nodiscard]] SafePtr<StorageBuffer> GetVertexBuffer() const { return VertexGPUBuffer; }
    [[nodiscard]] SafePtr<StorageBuffer> GetIndexBuffer() const { return IndexGPUBuffer; }
    [[nodiscard]] void* GetVertices() const { return Vertices; }
    [[nodiscard]] void* GetIndices() const { return Indices; }
    [[nodiscard]] vk::DescriptorSet GetDescSet() const { return DescSet; }

private:
    SafePtr<StorageBuffer>  VertexGPUBuffer{};
    SafePtr<StorageBuffer>  IndexGPUBuffer{};
    void*                   Vertices{};
    void*                   Indices{};

    uint32_t                VertexCount{};
    uint32_t                IndexCount{};

    vk::DescriptorSet       DescSet{};
private:
    friend class StaticMesh;

    void InitDescSet(GfxContext* ctx, vk::DescriptorSetLayout layout);

    Geometry() = default;
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

    std::vector<SubMesh>&       GetSubMeshes() { return m_SubMeshes; }
    const Geometry&             GetGeometry() const { return *m_Geometry.get(); }

    SafePtr<class Material>     GetMaterial(uint32_t index)
    {
        return m_Materials[index];
    }

    void                        SetMaterial(SafePtr<Material> mat, uint32_t index)
    {
        if (index > m_Materials.size()) return;
        m_Materials[index] = mat;
    }

    static SafePtr<StaticMesh> GenerateCube(uint32_t tesselationLevel);

    static SafePtr<StaticMesh> GenerateUVSphere(float radius = 1.f, uint32_t nLatitude = 32, uint32_t nLongitude = 32);

private:
    std::filesystem::path       m_Path{};
    std::vector<SubMesh>        m_SubMeshes{};

    std::unique_ptr<Geometry>   m_Geometry;
    uint32_t                    m_TotalVertexCount{};
    uint32_t                    m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<class Material>> m_Materials;
    SafePtr<class GfxPipeline> m_Pipeline;
    SafePtr<class GfxPipeline> m_TransparentPipeline;
    std::vector<SafePtr<class Texture>> m_Textures;
private:
    StaticMesh();

    void InitSubmeshes(const struct aiScene* scene);
    void LoadData(const struct aiScene* scene);
    void LoadMaterials(const struct aiScene* scene);
    void TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);
};

}
