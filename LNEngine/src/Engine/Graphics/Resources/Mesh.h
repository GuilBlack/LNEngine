#pragma once
#include "Engine/Graphics/Structs.h"
#include <unordered_map>
#include "Engine/Core/Utils/_Defines.h"
#include "Engine/Graphics/Resources/StorageBuffer.h"
#include "Engine/Graphics/Resources/Material.h"


namespace lne
{
class GfxContext;
class StorageBuffer;
class GfxTechnique;
class GfxPipeline;
class Material;
class Material;
class Texture;

struct Vertex
{
    glm::vec3 Position;
    glm::vec2 TexCoord;
    glm::vec3 Normal;
    glm::vec4 Tangent;
};

struct MeshletData
{
    u32             VertexOffset;
    u32             TriangleOffset;
    u32             VertexCount;
    u32             TriangleCount;

    glm::vec3       BoundsCenter;
    float           BoundsRadius;

    glm::i8vec3     ConeAxis;
    s8              ConeCutoff;
};

class Geometry
{
public:
    Geometry(GfxContext* ctx,
             SafePtr<StorageBuffer> vertexGPUBuffer, 
             SafePtr<StorageBuffer> indexGPUBuffer,
             void* vertices, void* indices,
             u32 vertexCount, u32 indexCount);

    Geometry(GfxContext* ctx,
             SafePtr<StorageBuffer> vertexGPUBuffer,
             SafePtr<StorageBuffer> indexGPUBuffer,
             SafePtr<StorageBuffer> meshletGPUBuffer,
             SafePtr<StorageBuffer> meshletVertexIndicesGPUBuffer,
             SafePtr<StorageBuffer> meshletTriangleIndicesGPUBuffer,
             void* meshlets, void* meshletVertexIndices, void* meshletTriangleIndices,
             void* vertices, u32 vertexCount, u32 meshletCount);

    ~Geometry();

    Geometry(Geometry&& other) noexcept;
    Geometry& operator=(Geometry&& other) noexcept;

    [[nodiscard]] u32          GetVertexCount() const { return m_VertexCount; }
    [[nodiscard]] u32          GetIndexCount() const { return m_IndexCount; }
    [[nodiscard]] SafePtr<StorageBuffer> GetVertexBuffer() const { return m_VertexGPUBuffer; }
    [[nodiscard]] SafePtr<StorageBuffer> GetIndexBuffer() const { return m_IndexGPUBuffer; }
    [[nodiscard]] void*             GetVertices() const { return m_Vertices; }
    [[nodiscard]] void*             GetIndices() const { return m_Indices; }
    [[nodiscard]] vk::DescriptorSet GetDescSet() const { return m_DescSet; }
    [[nodiscard]] u32          GetMeshletCount() const { return m_MeshletCount; }

    // abstract it as an interface instead of using enum checks?
    [[nodiscard]] GeometryType::Enum GetType() const
    {
        return m_Type;
    }

private:
    GeometryType::Enum      m_Type{ GeometryType::eClassic };
    SafePtr<StorageBuffer>  m_VertexGPUBuffer{};
    SafePtr<StorageBuffer>  m_IndexGPUBuffer{};
    void*                   m_Vertices{};
    void*                   m_Indices{};

    SafePtr<StorageBuffer>  m_MeshletGPUBuffer{};
    SafePtr<StorageBuffer>  m_MeshletVertexIndicesGPUBuffer{};
    SafePtr<StorageBuffer>  m_MeshletTriangleIndicesGPUBuffer{};
    void*                   m_Meshlets{};
    void*                   m_MeshletVertexIndices{};
    void*                   m_MeshletTriangleIndices{};

    u32                m_VertexCount{};
    u32                m_IndexCount{};
    u32                m_MeshletCount{};

    vk::DescriptorSet       m_DescSet{};
private:
    friend class StaticMesh;

    void                            InitDescSet(GfxContext* ctx,
                                                vk::DescriptorSetLayout layout);

    Geometry() = default;
    Geometry(const Geometry&) = delete;
    Geometry& operator=(const Geometry&) = delete;
    Geometry& operator=(Geometry&) = delete;
    Geometry operator=(Geometry) = delete;
};

class StaticMesh : public RefCountBase
{
public:
    // TODO: probably make a mesh importer class or something
    StaticMesh(std::filesystem::path path, GeometryType::Enum geometryType = GeometryType::eMeshlet);

    std::vector<SubMesh>&               GetSubMeshes() { return m_SubMeshes; }
    const Geometry&                     GetGeometry() const { return *m_Geometry.get(); }

    [[nodiscard]] SafePtr<Material>     GetMaterial(u32 index)
    {
        return m_Materials[index];
    }

    void                                SetMaterial(SafePtr<Material> mat,
                                                    u32 index);

    [[nodiscard]] static SafePtr<StaticMesh> GenerateCube(u32 tesselationLevel);
    [[nodiscard]] static SafePtr<StaticMesh> GenerateUVSphere(float radius = 1.f,
                                                              u32 nLatitude = 32,
                                                              u32 nLongitude = 32);

    static SafePtr<StaticMesh> GenerateUVSphereMeshlets(float radius = 1.f,
                                                                 u32 nLatitude = 32,
                                                                 u32 nLongitude = 32);

    SafePtr<StaticMesh>                 Clone() const;

private:
    std::filesystem::path                   m_Path{};

    std::vector<SubMesh>                    m_SubMeshes{};
    std::shared_ptr<Geometry>               m_Geometry;
    u32                                m_TotalVertexCount{};
    u32                                m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<Material>>          m_Materials;
private:
    StaticMesh();

    void                                InitSubmeshes(const struct aiScene* scene);
    void                                LoadAsClassicMesh(const aiScene* scene);
    void                                LoadAsMeshlets(const aiScene* scene);
    void                                FillMeshCPUData(const aiScene* scene);

    void LoadMaterials(const struct aiScene* scene, GeometryType::Enum geometryType);
    void                                TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);

    static void                         GenerateUVSphereData(u32 nLatitude, u32 nLongitude,
                                                             float radius,
                                                             Vertex* oVertices, u32* oIndices,
                                                             u32 nVertices);
};

}
