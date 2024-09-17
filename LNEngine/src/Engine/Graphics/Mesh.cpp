#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/aabb.h>
#include "Core/SafePtr.h"
#include "Core/Utils/Log.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"
#include "Core/Utils/_Defines.h"
#include "Graphics/Pipeline.h"
#include "Graphics/Material.h"
#include "Graphics/Texture.h"
#include "Graphics/DynamicDescriptorAllocator.h"

#include "Mesh.h"

lne::StaticMesh::StaticMesh(std::filesystem::path path, SafePtr<GfxPipeline> pipeline)
    : m_Path(path), m_Pipeline(pipeline)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices);

    if (!scene)
    {
        LNE_ERROR("Assimp error: {0}", importer.GetErrorString());
        return;
    }

    if (!scene->HasMeshes())
    {
        LNE_ERROR("No meshes found in file: {0}", path.string());
        return;
    }

    uint32_t totalVertexCount = 0;
    uint32_t totalIndexCount = 0;

    InitSubmeshes(scene);
    LoadData(scene);
}

void lne::StaticMesh::InitSubmeshes(const aiScene* scene)
{
    m_SubMeshes.reserve(scene->mNumMeshes);

    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        bool skip = !mesh->HasPositions() || !mesh->HasNormals();
        SubMesh submesh{
            .BaseVertex = m_TotalVertexCount,
            .BaseIndex = m_TotalIndexCount,
            .VertexCount = skip ? 0 : mesh->mNumVertices,
            .IndexCount = skip ? 0 : mesh->mNumFaces * 3,
            .MaterialIndex = mesh->mMaterialIndex,
            .BoundingBox = AABB{
                .Min = { mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z },
                .Max = { mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z }
            },
            .Name = mesh->mName.C_Str()
        };
        m_SubMeshes.emplace_back(submesh);

        m_TotalVertexCount += mesh->mNumVertices;
        m_TotalIndexCount += mesh->mNumFaces * 3;
    }

    TraverseNodes(scene->mRootNode, glm::mat4(1.0f));

    m_Vertices.reserve(m_TotalVertexCount);
    m_Indices.reserve(m_TotalIndexCount);
}

void lne::StaticMesh::LoadData(const aiScene* scene)
{
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!mesh->HasPositions() || !mesh->HasNormals())
            continue;

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            Vertex vertex;
            vertex.Position = glm::vec3(m_SubMeshes[m].WorldTransform * glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f));
            vertex.Normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };
            if (mesh->HasTextureCoords(0))
                vertex.TexCoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };

            m_Vertices.push_back(vertex);
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            LNE_ASSERT(mesh->mFaces[f].mNumIndices == 3, "Face is not a triangle");

            const aiFace& face = mesh->mFaces[f];
            for (uint32_t i = 0; i < face.mNumIndices; ++i)
                m_Indices.push_back(face.mIndices[i]);
        }
    }

    LNE_ASSERT(m_Vertices.size() == m_TotalVertexCount, "Vertex count mismatch");
    LNE_ASSERT(m_Indices.size() == m_TotalIndexCount, "Index count mismatch");

    m_Geometry.VertexCount = m_TotalVertexCount;
    m_Geometry.IndexCount = m_TotalIndexCount;

    auto& renderer = ApplicationBase::GetRenderer();

    m_Geometry.VertexGPUBuffer = renderer.CreateGeometryBuffer(m_Vertices.data(), m_Vertices.size() * sizeof(Vertex));
    m_Geometry.IndexGPUBuffer = renderer.CreateGeometryBuffer(m_Indices.data(), m_Indices.size() * sizeof(uint32_t));
    
    LoadMaterials(scene);
}

void lne::StaticMesh::LoadMaterials(const aiScene* scene)
{
    auto& renderer = ApplicationBase::GetRenderer();

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* aiMat = scene->mMaterials[i];
        aiString name;
        aiMat->Get(AI_MATKEY_NAME, name);

        LNE_INFO("Material: {0}", name.C_Str());

        SafePtr<Material> material = SafePtr<Material>(lnnew Material(m_Pipeline));
        m_Materials.push_back(material);

        aiString texturePath;

        aiColor3D aiColor(1.0f);

        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
        {
            glm::vec4 color = { aiColor.r, aiColor.g, aiColor.b, 1.0f };
            material->SetProperty("uColor", color);
        }

        float roughness, metallic;
        if (aiMat->Get(AI_MATKEY_REFLECTIVITY, metallic) != AI_SUCCESS)
            metallic = 0.0f;
        material->SetProperty("uMetalness", metallic);

        if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS)
            roughness = 0.4f;
        material->SetProperty("uRoughness", roughness);

        bool hasAlbedoTex = aiMat->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &texturePath) == AI_SUCCESS;

        if (!hasAlbedoTex)
            hasAlbedoTex = aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS;

        if (hasAlbedoTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / texturePath.C_Str();
            if (!std::filesystem::exists(texPath))
            {
                LNE_WARN("Texture not found: {0}", texPath.string());
            }
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string());
                material->SetTexture("tAlbedo", texture);
                m_Textures.push_back(texture);
            }
        }
    }
}

void lne::StaticMesh::TraverseNodes(const aiNode* node, const glm::mat4& parentTransform)
{
    glm::mat4 transform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));
    glm::mat4 worldTransform = parentTransform * transform;

    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        uint32_t meshIndex = node->mMeshes[i];
        m_SubMeshes[meshIndex].WorldTransform = worldTransform;
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        TraverseNodes(node->mChildren[i], worldTransform);
}
