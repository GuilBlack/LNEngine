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

class Geometry
{
public:
    Geometry(GfxContext* ctx,
             SafePtr<StorageBuffer> vertexGPUBuffer, 
             SafePtr<StorageBuffer> indexGPUBuffer,
             void* vertices, void* indices,
             uint32_t vertexCount, uint32_t indexCount);


    Geometry(GfxContext* ctx,
             SafePtr<StorageBuffer> vertexGPUBuffer,
             SafePtr<StorageBuffer> indexGPUBuffer,
             SafePtr<StorageBuffer> meshletGPUBuffer,
             SafePtr<StorageBuffer> meshletVertexIndicesGPUBuffer,
             SafePtr<StorageBuffer> meshletTriangleIndicesGPUBuffer,
             void* vertices, uint32_t vertexCount, uint32_t meshletCount);

    ~Geometry();

    Geometry(Geometry&& other) noexcept;
    Geometry& operator=(Geometry&& other) noexcept;

    [[nodiscard]] uint32_t          GetVertexCount() const { return m_VertexCount; }
    [[nodiscard]] uint32_t          GetIndexCount() const { return m_IndexCount; }
    [[nodiscard]] SafePtr<StorageBuffer> GetVertexBuffer() const { return m_VertexGPUBuffer; }
    [[nodiscard]] SafePtr<StorageBuffer> GetIndexBuffer() const { return m_IndexGPUBuffer; }
    [[nodiscard]] void*             GetVertices() const { return m_Vertices; }
    [[nodiscard]] void*             GetIndices() const { return m_Indices; }
    [[nodiscard]] vk::DescriptorSet GetDescSet() const { return m_DescSet; }

    // abstract it as an interface instead of using enum checks?
    [[nodiscard]] GeometryType::Enum GetType() const
    {
        return GeometryType::eClassic;
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

    uint32_t                m_VertexCount{};
    uint32_t                m_IndexCount{};
    uint32_t                m_MeshletCount{};

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

struct SubMesh
{
    std::string Name;
    uint32_t BaseVertex;
    uint32_t BaseIndex;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t MaterialIndex;
    AABB BoundingBox;

    glm::mat4 WorldTransform = glm::mat4(1.0f);
};

class StaticMesh : public RefCountBase
{
public:
    // TODO: probably make a mesh importer class or something
    StaticMesh(std::filesystem::path path);

    std::vector<SubMesh>&               GetSubMeshes() { return m_SubMeshes; }
    const Geometry&                     GetGeometry() const { return *m_Geometry.get(); }

    [[nodiscard]] SafePtr<Material>     GetMaterial(uint32_t index)
    {
        return m_Materials[index];
    }

    void                                SetMaterial(SafePtr<Material> mat, 
                                                      uint32_t index)
    {
        if (index >= m_Materials.size())
        {
            LNE_WARN("Material index out of bounds");
            return;
        }
        m_Materials[index] = mat;
    }



    [[nodiscard]] static SafePtr<StaticMesh> GenerateCube(uint32_t tesselationLevel);
    [[nodiscard]] static SafePtr<StaticMesh> GenerateUVSphere(float radius = 1.f,
                                                              uint32_t nLatitude = 32,
                                                              uint32_t nLongitude = 32);

    static void                         GenerateUVSphereMeshlets(float radius = 1.f,
                                                                 uint32_t nLatitude = 32,
                                                                 uint32_t nLongitude = 32);

    SafePtr<StaticMesh>                 Clone() const;

private:
    std::filesystem::path                   m_Path{};

    std::vector<SubMesh>                    m_SubMeshes{};
    std::shared_ptr<Geometry>               m_Geometry;
    uint32_t                                m_TotalVertexCount{};
    uint32_t                                m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<Material>>          m_Materials;
private:
    StaticMesh();

    void                                InitSubmeshes(const struct aiScene* scene);
    void                                LoadData(const struct aiScene* scene);
    void                                LoadMaterials(const struct aiScene* scene);
    void                                TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);

    static void                         GenerateUVSphereData(uint32_t nLatitude, uint32_t nLongitude,
                                                             float radius,
                                                             Vertex* oVertices, uint32_t* oIndices,
                                                             uint32_t nVertices);
};

}
