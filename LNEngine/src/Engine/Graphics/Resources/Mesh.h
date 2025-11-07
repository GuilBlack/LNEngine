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

    ~Geometry();

    Geometry(Geometry&& other) noexcept;
    Geometry& operator=(Geometry&& other) noexcept;

    [[nodiscard]] uint32_t          GetVertexCount() const { return VertexCount; }
    [[nodiscard]] uint32_t          GetIndexCount() const { return IndexCount; }
    [[nodiscard]] SafePtr<StorageBuffer> GetVertexBuffer() const { return VertexGPUBuffer; }
    [[nodiscard]] SafePtr<StorageBuffer> GetIndexBuffer() const { return IndexGPUBuffer; }
    [[nodiscard]] void*             GetVertices() const { return Vertices; }
    [[nodiscard]] void*             GetIndices() const { return Indices; }
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
    StaticMesh(std::filesystem::path path,
               SafePtr<GfxTechnique> opaqueTechnique,
               SafePtr<GfxTechnique> transparentTechnique);

    std::vector<SubMesh>&               GetSubMeshes() { return m_SubMeshes; }
    const Geometry&                     GetGeometry() const { return *m_Geometry.get(); }

    [[nodiscard]] SafePtr<Material>   GetMaterial(uint32_t index)
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

private:
    std::filesystem::path                   m_Path{};

    std::vector<SubMesh>                    m_SubMeshes{};
    std::unique_ptr<Geometry>               m_Geometry;
    uint32_t                                m_TotalVertexCount{};
    uint32_t                                m_TotalIndexCount{};

    // TODO: move to a resource manager
    std::vector<SafePtr<Material>>        m_Materials;
    SafePtr<GfxPipeline>                    m_Pipeline;
    SafePtr<GfxPipeline>                    m_TransparentPipeline;
    SafePtr<GfxTechnique>                   m_OpaqueTechnique;
    SafePtr<GfxTechnique>                   m_TransparentTechnique;
    std::vector<SafePtr<Texture>>           m_Textures;
private:
    StaticMesh();

    void InitSubmeshes(const struct aiScene* scene);
    void LoadData(const struct aiScene* scene);
    void LoadMaterials(const struct aiScene* scene);
    void TraverseNodes(const struct aiNode* node, const glm::mat4& parentTransform);
};

}
