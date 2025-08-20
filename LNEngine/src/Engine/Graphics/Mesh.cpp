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
#include <stb/stb_image.h>

namespace lne
{
StaticMesh::StaticMesh(std::filesystem::path path, SafePtr<GfxPipeline> pipeline, SafePtr<GfxPipeline> transparentPipeline)
    : m_Path(path), 
    m_Geometry(nullptr), 
    m_Materials{}, 
    m_Pipeline(pipeline), 
    m_TransparentPipeline(transparentPipeline),
    m_Textures{}
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path.string(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace);

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

    m_Geometry.reset(lnnew Geometry());
    InitSubmeshes(scene);
    LoadData(scene);
}

StaticMesh::StaticMesh()
{
    m_Materials.resize(1);
}

void StaticMesh::InitSubmeshes(const aiScene* scene)
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

    m_Geometry->Vertices = lnnew Vertex[m_TotalVertexCount];
    m_Geometry->Indices = lnnew uint32_t[m_TotalIndexCount];
}

void StaticMesh::LoadData(const aiScene* scene)
{
    LoadMaterials(scene);

    uint32_t indexIndex = 0;
    uint32_t vertexIndex = 0;
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!mesh->HasPositions() || !mesh->HasNormals() || !mesh->HasTangentsAndBitangents())
            continue;

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            Vertex vertex;
            vertex.Position = glm::vec3(m_SubMeshes[m].WorldTransform * glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f));
            vertex.Normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };

            glm::vec3 tangent = { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
            glm::vec3 bitangent = { mesh->mBitangents[v].x, mesh->mBitangents[v].y, mesh->mBitangents[v].z };
            float handedness = (glm::dot(glm::cross(vertex.Normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
            vertex.Tangent = glm::vec4(tangent, handedness);

            if (mesh->HasTextureCoords(0))
                vertex.TexCoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
            LNE_ASSERT(vertexIndex < m_TotalVertexCount, "Vertex index out of bounds");
            ((Vertex*)m_Geometry->Vertices)[vertexIndex++] = vertex;
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            LNE_ASSERT(mesh->mFaces[f].mNumIndices == 3, "Face is not a triangle");

            const aiFace& face = mesh->mFaces[f];
            for (uint32_t i = 0; i < face.mNumIndices; ++i)
            {
                LNE_ASSERT(indexIndex < m_TotalIndexCount, "Index index out of bounds");
                ((uint32_t*)m_Geometry->Indices)[indexIndex++] = face.mIndices[i] + m_SubMeshes[m].BaseVertex;
            }
        }
    }
    LNE_ASSERT(indexIndex == m_TotalIndexCount, "Index count mismatch");
    LNE_ASSERT(vertexIndex == m_TotalVertexCount, "Vertex count mismatch");

    m_Geometry->VertexCount = m_TotalVertexCount;
    m_Geometry->IndexCount = m_TotalIndexCount;

    auto& renderer = ApplicationBase::GetRenderer();

    m_Geometry->VertexGPUBuffer = renderer.CreateGeometryBuffer(m_Geometry->Vertices, m_TotalVertexCount * sizeof(Vertex));
    m_Geometry->IndexGPUBuffer = renderer.CreateGeometryBuffer(m_Geometry->Indices, m_TotalIndexCount * sizeof(uint32_t));

    SafePtr ctx = renderer.GetGfxContext();
    m_Geometry->InitDescSet(ctx.GetPtr(), ctx->GetGeometryDescriptorSetLayout());
}

void StaticMesh::LoadMaterials(const aiScene* scene)
{
    auto& renderer = ApplicationBase::GetRenderer();

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* aiMat = scene->mMaterials[i];
        aiString name;
        aiMat->Get(AI_MATKEY_NAME, name);

        aiString texturePath;

        bool hasColTex = aiMat->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &texturePath) == AI_SUCCESS;

        if (!hasColTex)
            hasColTex = aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS;

        bool isTransparent{ false };
        if (hasColTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / texturePath.C_Str();
            if (!std::filesystem::exists(texPath) || texturePath.C_Str() == "")
            {
                LNE_WARN("Texture not found: {0}", texPath.string());
            }
            else
            {
                int x, y, comp;
                stbi_info(texPath.string().c_str(), &x, &y, &comp);
                isTransparent = (comp == 4);
            }
        }

        LNE_INFO("Material: {0}", name.C_Str());

        
        SafePtr<Material> material{};
        if (isTransparent)
            material = SafePtr<Material>(lnnew Material(m_TransparentPipeline));
        else
            material = SafePtr<Material>(lnnew Material(m_Pipeline));
        material->SetTransparency(isTransparent);
        m_Materials.push_back(material);

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

        if (hasColTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / texturePath.C_Str();
            if (!std::filesystem::exists(texPath) || texturePath.length == 0)
            {
                LNE_WARN("Albedo texture not found for mat: {0}", aiMat->GetName().C_Str());
            }
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string());
                material->SetTexture("tAlbedo", texture);
                m_Textures.push_back(texture);
            }
        }

        aiString metalTex{};
        bool hasMetTex = aiMat->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &metalTex) == AI_SUCCESS;
        if (hasMetTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / metalTex.C_Str();
            if (!std::filesystem::exists(texPath) || metalTex.length == 0)
            {
                LNE_WARN("Metalness texture not found for mat: {0}", aiMat->GetName().C_Str());
            }
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                material->SetTexture("tMetalness", texture);
                m_Textures.push_back(texture);
            }
        }

        aiString roughTex{};
        bool hasRoughTex = aiMat->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &roughTex) == AI_SUCCESS;

        if (hasRoughTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / roughTex.C_Str();
            if (!std::filesystem::exists(texPath) || roughTex.length == 0)
            {
                LNE_WARN("Roughness texture not found for mat: {0}", aiMat->GetName().C_Str());
            }
            else
            {
                if (roughTex != metalTex)
                {
                    SafePtr<Texture> texture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                    material->SetTexture("tRoughness", texture);
                    m_Textures.push_back(texture);
                }
                else
                    material->SetTexture("tRoughness", m_Textures.back());
            }
        }

        aiString normalTex{};
        bool hasNormalTex{ false };
        for (unsigned int i = 0; i < aiMat->GetTextureCount(aiTextureType_NORMALS); i++)
        {
            aiMat->GetTexture(aiTextureType_NORMALS, 0, &normalTex);
            if (normalTex.length > 0)
            {
                hasNormalTex = true;
            }
        }
        if (hasNormalTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / normalTex.C_Str();
            if (!std::filesystem::exists(texPath) || normalTex.length == 0)
            {
                LNE_WARN("Normal map not found for mat: {0}", aiMat->GetName().C_Str());
            }
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                material->SetTexture("tNormal", texture);
                m_Textures.push_back(texture);
            }
        }
    }
}

void StaticMesh::TraverseNodes(const aiNode* node, const glm::mat4& parentTransform)
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

SafePtr<StaticMesh> StaticMesh::GenerateCube(uint32_t tesselationLevel)
{
    float step = 2.0f / tesselationLevel;
    Geometry* geometry = lnnew Geometry();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    auto addQuad = [&](glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec3 normal)
        {
            uint32_t startIndex = (uint32_t)vertices.size();
            vertices.push_back({ p0, {0.0f, 1.0f}, normal, {} });
            vertices.push_back({ p1, {1.0f, 1.0f}, normal, {} });
            vertices.push_back({ p2, {1.0f, 0.0f}, normal, {} });
            vertices.push_back({ p3, {0.0f, 0.0f}, normal, {} });

            indices.push_back(startIndex + 0);
            indices.push_back(startIndex + 1);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 2);
            indices.push_back(startIndex + 3);
            indices.push_back(startIndex + 0);
        };

    for (uint32_t i = 0; i < tesselationLevel; ++i)
    {
        for (uint32_t j = 0; j < tesselationLevel; ++j)
        {
            float x0 = -1.0f + i * step;
            float x1 = x0 + step;
            float y0 = -1.0f + j * step;
            float y1 = y0 + step;

            // Front face
            addQuad({ x0, y0, 1.0f }, { x1, y0, 1.0f }, { x1, y1, 1.0f }, { x0, y1, 1.0f }, { 0.0f, 0.0f, 1.0f });
            // Back face
            addQuad({ x1, y0, -1.0f }, { x0, y0, -1.0f }, { x0, y1, -1.0f }, { x1, y1, -1.0f }, { 0.0f, 0.0f, -1.0f });
            // Left face
            addQuad({ -1.0f, y0, x0 }, { -1.0f, y0, x1 }, { -1.0f, y1, x1 }, { -1.0f, y1, x0 }, { -1.0f, 0.0f, 0.0f });
            // Right face
            addQuad({ 1.0f, y0, x1 }, { 1.0f, y0, x0 }, { 1.0f, y1, x0 }, { 1.0f, y1, x1 }, { 1.0f, 0.0f, 0.0f });
            // Top face
            addQuad({ x0, 1.0f, y0 }, { x0, 1.0f, y1 }, { x1, 1.0f, y1 }, { x1, 1.0f, y0 }, { 0.0f, 1.0f, 0.0f });
            // Bottom face
            addQuad({ x0, -1.0f, y0 }, { x1, -1.0f, y0 }, { x1, -1.0f, y1 }, { x0, -1.0f, y1 }, { 0.0f, -1.0f, 0.0f });
        }
    }

    Renderer& renderer = ApplicationBase::GetRenderer();
    Vertex* verticesPtr = lnnew Vertex[vertices.size()];
    uint32_t* indicesPtr = lnnew uint32_t[indices.size()];
    std::memcpy(verticesPtr, vertices.data(), vertices.size() * sizeof(Vertex));
    std::memcpy(indicesPtr, indices.data(), indices.size() * sizeof(uint32_t));

    geometry->VertexGPUBuffer = renderer.CreateGeometryBuffer(verticesPtr, vertices.size() * sizeof(Vertex));
    geometry->IndexGPUBuffer = renderer.CreateGeometryBuffer(indicesPtr, indices.size() * sizeof(uint32_t));
    geometry->Vertices = verticesPtr;
    geometry->Indices = indicesPtr;
    geometry->VertexCount = (uint32_t)vertices.size();
    geometry->IndexCount = (uint32_t)indices.size();

    SafePtr ctx = renderer.GetGfxContext();
    geometry->InitDescSet(ctx.GetPtr(), ctx->GetGeometryDescriptorSetLayout());

    SafePtr<StaticMesh> mesh = lnnew StaticMesh();
    mesh->m_Geometry.reset(geometry);

    return mesh;
}

SafePtr<StaticMesh> StaticMesh::GenerateUVSphere(float radius, uint32_t nLatitude, uint32_t nLongitude)
{
    if (nLatitude < 1)
        nLatitude = 1;
    if (nLongitude < 3)
        nLongitude = 3;

    Geometry* geometry = lnnew Geometry();

    uint32_t nVertices = nLatitude * (nLongitude + 1) + (nLongitude * 2);
    //-1 to nLat because it wouldn't make sense otherwise.
    uint32_t nIndices = 2 * 3 * nLongitude + 2 * 3 * (nLatitude - 1) * nLongitude;

    Vertex* vertices = lnnew Vertex[nVertices];
    uint32_t* indices = lnnew uint32_t[nIndices];

    // here, latitude points should be mapped between -90 and 90 degrees (or -PI/2 to PI/2).
    // +1 to nLat because it wouldn't make sense otherwise.
    float latitudeSlope = glm::pi<float>() / (float)(nLatitude + 1);
    // here, longitude points should be mapped between -180 and 180 degrees (or -PI to PI).
    float longitudeSlope = (2.f * glm::pi<float>()) / (float)nLongitude;

    uint32_t count = 0;
    // add north pole
    for (uint32_t i = 1; i <= nLongitude; ++i)
    {
        vertices[count].Position = { 0.0f, radius, 0.0f };
        vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 0.0f };
        vertices[count].Normal = { 0.0f, 1.0f, 0.0f };
        ++count;
    }

    //middle quads
    for (uint32_t i = 1; i < (nLatitude + 1); ++i)
    {
        float pLat = latitudeSlope * (float)i;
        for (uint32_t j = 0; j < nLongitude + 1; ++j)
        {
            float pLon = longitudeSlope * (float)j;
            glm::vec3 point = { sinf(pLat) * cosf(pLon), cosf(pLat), sinf(pLat) * sinf(pLon) };

            vertices[count].Position = { radius * point.x, radius * point.y, radius * point.z };
            vertices[count].TexCoord = { 1 - (float)j / (float)nLongitude, (float)i / (float)(nLatitude + 1) };
            vertices[count].Normal = glm::vec3(point);

            ++count;
        }
    }

    //add south pole
    for (uint32_t i = 1; i <= nLongitude; ++i)
    {
        vertices[count].Position = { 0.0f, -radius, 0.0f };
        vertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 1.0f };
        vertices[count].Normal = { 0.0f, -1.0f, 0.0f };
        ++count;
    }

    count = 0;
    //north pole indices
    for (uint32_t i = 0; i < nLongitude; ++i)
    {
        indices[count++] = i;
        indices[count++] = (nLongitude - 1) + i + 2;
        indices[count++] = (nLongitude - 1) + i + 1;
    }

    //middle quads
    for (uint32_t i = 0; i < nLatitude - 1; ++i)
    {
        for (uint32_t j = 0; j < nLongitude; ++j)
        {
            uint32_t index[4] = {
                nLongitude + i * (nLongitude + 1) + j,
                nLongitude + i * (nLongitude + 1) + (j + 1),
                nLongitude + (i + 1) * (nLongitude + 1) + (j + 1),
                nLongitude + (i + 1) * (nLongitude + 1) + j
            };

            indices[count++] = index[0];
            indices[count++] = index[1];
            indices[count++] = index[2];

            indices[count++] = index[0];
            indices[count++] = index[2];
            indices[count++] = index[3];
        }
    }

    //south pole indices
    const uint32_t southPoleIndex = nVertices - nLongitude;
    for (uint32_t i = 0; i < nLongitude; ++i)
    {
        indices[count++] = southPoleIndex + i;
        indices[count++] = southPoleIndex - (nLongitude + 1) + i;
        indices[count++] = southPoleIndex - (nLongitude + 1) + i + 1;
    }

    Renderer& renderer = ApplicationBase::GetRenderer();
    geometry->VertexGPUBuffer = renderer.CreateGeometryBuffer(vertices, nVertices * sizeof(Vertex));
    geometry->IndexGPUBuffer = renderer.CreateGeometryBuffer(indices, nIndices * sizeof(uint32_t));

    geometry->VertexCount = nVertices;
    geometry->IndexCount = nIndices;

    geometry->Vertices = vertices;
    geometry->Indices = indices;

    SafePtr ctx = renderer.GetGfxContext();
    geometry->InitDescSet(ctx.GetPtr(), ctx->GetGeometryDescriptorSetLayout());
    
    SafePtr<StaticMesh> mesh = lnnew StaticMesh();
    mesh->m_Geometry.reset(geometry);
    return mesh;
}

Geometry::Geometry(GfxContext* ctx, SafePtr<StorageBuffer> vertexGPUBuffer, SafePtr<StorageBuffer> indexGPUBuffer, void* vertices, void* indices, uint32_t vertexCount, uint32_t indexCount)
    : VertexGPUBuffer(vertexGPUBuffer), IndexGPUBuffer(indexGPUBuffer), 
      Vertices(vertices), Indices(indices), 
      VertexCount(vertexCount), IndexCount(indexCount)
{
    InitDescSet(ctx, ctx->GetGeometryDescriptorSetLayout());
}

Geometry::Geometry(Geometry&& other) noexcept
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

Geometry& Geometry::operator=(Geometry&& other) noexcept
{
    if (this == &other)
        return *this;
    VertexGPUBuffer = std::move(other.VertexGPUBuffer);
    IndexGPUBuffer = std::move(other.IndexGPUBuffer);
    Vertices = other.Vertices;
    Indices = other.Indices;
    VertexCount = other.VertexCount;
    IndexCount = other.IndexCount;
    other.Vertices = nullptr;
    other.Indices = nullptr;
    other.IndexCount = 0;
    other.VertexCount = 0;
    return *this;
}

Geometry::~Geometry()
{
    delete[] Vertices;
    delete[] Indices;
    VertexGPUBuffer.Reset();
    IndexGPUBuffer.Reset();
    if (DescSet)
    {
        DescriptorSetDeletion resourceDeletion{
            .Type = DescriptorType::eStorageOnly,
            .DescriptorSet = DescSet,
        };
        ApplicationBase::GetRenderer().GetGfxContext()->EnqueueResourceDeletion(ResourceDeletion{
            .Type = ResourceType::eDescriptorSet,
            .Resource = resourceDeletion
        });
    }
}

void Geometry::InitDescSet(GfxContext* ctx, vk::DescriptorSetLayout layout)
{
    DescSet = ctx->AllocateDescriptorSet(layout, DescriptorType::eStorageOnly);
    vk::DescriptorBufferInfo vertexInfo = VertexGPUBuffer->GetDescriptorInfo();
    vk::DescriptorBufferInfo indexInfo = IndexGPUBuffer->GetDescriptorInfo();
    std::vector<vk::WriteDescriptorSet> writeDescSets = {
        vk::WriteDescriptorSet{
            DescSet,
            0,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &vertexInfo,
            nullptr
        },
        vk::WriteDescriptorSet{
            DescSet,
            1,
            0,
            1,
            vk::DescriptorType::eStorageBuffer,
            nullptr,
            &indexInfo,
            nullptr
        }
    };
    ctx->GetDevice().updateDescriptorSets(writeDescSets, {});
}

}
