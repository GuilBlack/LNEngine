#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/aabb.h>
#include <meshopt/src/meshoptimizer.h>
#include <stb/stb_image.h>

#include "Core/SafePtr.h"
#include "Core/Utils/Log.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Renderer.h"
#include "Core/Utils/_Defines.h"
#include "Graphics/Resources/Pipeline.h"
#include "Graphics/Resources/Effect.h"
#include "Graphics/Resources/GfxTechnique.h"
#include "Graphics/Resources/Material.h"
#include "Graphics/Resources/Texture.h"
#include "Graphics/DynamicDescriptorAllocator.h"

#include "Mesh.h"

namespace lne
{
StaticMesh::StaticMesh()
{
    m_Materials.resize(1);
}

StaticMesh::StaticMesh(std::filesystem::path path)
    : m_Path(path),
    m_Geometry{ nullptr }
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

void StaticMesh::InitSubmeshes(const aiScene* scene)
{
    m_SubMeshes.reserve(scene->mNumMeshes);
    std::vector<aiMesh*> meshes;
    std::unordered_map<std::string, uint32_t> duplicatedMeshes;
    for (uint32_t i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[i];
        meshes.push_back(mesh);
        bool skip = !mesh->HasPositions() || !mesh->HasNormals();
        if (skip)
            LNE_WARN("Mesh '{}' skipped: missing positions or normals", mesh->mName.C_Str());
        SubMesh submesh;
        if (duplicatedMeshes.contains(mesh->mName.C_Str()))
        {
            auto& originalSubmesh = m_SubMeshes[duplicatedMeshes[mesh->mName.C_Str()]];
            submesh = {
                .Name = mesh->mName.C_Str(),
                .BaseVertex = originalSubmesh.BaseVertex,
                .BaseIndex = originalSubmesh.BaseIndex,
                .VertexCount = originalSubmesh.VertexCount,
                .IndexCount = originalSubmesh.IndexCount,
                .MaterialIndex = mesh->mMaterialIndex,
                .BoundingBox = originalSubmesh.BoundingBox,
            };
        }
        else
        {
            submesh = {
                .Name = mesh->mName.C_Str(),
                .BaseVertex = m_TotalVertexCount,
                .BaseIndex = m_TotalIndexCount,
                .VertexCount = skip ? 0 : mesh->mNumVertices,
                .IndexCount = skip ? 0 : mesh->mNumFaces * 3,
                .MaterialIndex = mesh->mMaterialIndex,
                .BoundingBox = AABB{
                    .Min = { mesh->mAABB.mMin.x, mesh->mAABB.mMin.y, mesh->mAABB.mMin.z },
                    .Max = { mesh->mAABB.mMax.x, mesh->mAABB.mMax.y, mesh->mAABB.mMax.z }
                },
            };
            duplicatedMeshes[mesh->mName.C_Str()] = static_cast<uint32_t>(m_SubMeshes.size());

            m_TotalVertexCount += mesh->mNumVertices;
            m_TotalIndexCount += mesh->mNumFaces * 3;
        }

        m_SubMeshes.emplace_back(submesh);
    }

    TraverseNodes(scene->mRootNode, glm::mat4(1.0f));

    m_Geometry->m_Vertices = lnnew Vertex[m_TotalVertexCount];
    m_Geometry->m_Indices = lnnew uint32_t[m_TotalIndexCount];
}

void StaticMesh::LoadData(const aiScene* scene)
{
    LoadMaterials(scene);

    uint32_t indexIndex = 0;
    uint32_t vertexIndex = 0;
    std::unordered_set<std::string> duplicatedMeshes;
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        if (!mesh->HasPositions() || !mesh->HasNormals())
            continue;
        if (duplicatedMeshes.contains(mesh->mName.C_Str()))
            continue;
        else
            duplicatedMeshes.insert(mesh->mName.C_Str());
        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            Vertex vertex;
            vertex.Position = glm::vec3(m_SubMeshes[m].WorldTransform * glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f));
            vertex.Normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };

            if (mesh->HasTangentsAndBitangents())
            {
                glm::vec3 tangent = { mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z };
                glm::vec3 bitangent = { mesh->mBitangents[v].x, mesh->mBitangents[v].y, mesh->mBitangents[v].z };
                float handedness = (glm::dot(glm::cross(vertex.Normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
                vertex.Tangent = glm::vec4(tangent, handedness);
            }
            else
                vertex.Tangent = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

            if (mesh->HasTextureCoords(0))
                vertex.TexCoord = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
            LNE_ASSERT(vertexIndex < m_TotalVertexCount, "Vertex index out of bounds");
            ((Vertex*)m_Geometry->m_Vertices)[vertexIndex++] = vertex;
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            LNE_ASSERT(mesh->mFaces[f].mNumIndices == 3, "Face is not a triangle");

            const aiFace& face = mesh->mFaces[f];
            for (uint32_t i = 0; i < face.mNumIndices; ++i)
            {
                LNE_ASSERT(indexIndex < m_TotalIndexCount, "Index index out of bounds");
                ((uint32_t*)m_Geometry->m_Indices)[indexIndex++] = face.mIndices[i] + m_SubMeshes[m].BaseVertex;
            }
        }
    }
    LNE_ASSERT(indexIndex == m_TotalIndexCount, "Index count mismatch");
    LNE_ASSERT(vertexIndex == m_TotalVertexCount, "Vertex count mismatch");

    m_Geometry->m_VertexCount = m_TotalVertexCount;
    m_Geometry->m_IndexCount = m_TotalIndexCount;

    auto& renderer = ApplicationBase::GetRenderer();

    m_Geometry->m_VertexGPUBuffer = renderer.CreateGeometryBuffer(m_Geometry->m_Vertices, m_TotalVertexCount * sizeof(Vertex));
    m_Geometry->m_IndexGPUBuffer = renderer.CreateGeometryBuffer(m_Geometry->m_Indices, m_TotalIndexCount * sizeof(uint32_t));

    SafePtr ctx = renderer.GetGfxContext();
    m_Geometry->InitDescSet(ctx.GetPtr(), ctx->GetStorageOnlyDescriptorSetLayout(2));
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
            }
        }

        LNE_INFO("Material: {0}", name.C_Str());

        SafePtr<Material> material{};
        if (isTransparent)
            material = lnnew Material(renderer.GetTechnique("DefaultMeshTransparent"));
        else
            material = lnnew Material(renderer.GetTechnique("DefaultMeshOpaque"));
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
                LNE_WARN("Albedo texture not found for mat: {0}", aiMat->GetName().C_Str());
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string());
                material->SetTexture("tAlbedo", texture);
            }
        }

        aiString metalTex{};
        SafePtr<Texture> metalTexture = nullptr;
        bool hasMetTex = aiMat->GetTexture(AI_MATKEY_METALLIC_TEXTURE, &metalTex) == AI_SUCCESS;
        if (hasMetTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / metalTex.C_Str();
            if (!std::filesystem::exists(texPath) || metalTex.length == 0)
                LNE_WARN("Metalness texture not found for mat: {0}", aiMat->GetName().C_Str());
            else
            {
                metalTexture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                material->SetTexture("tMetalness", metalTexture);
            }
        }

        aiString roughTex{};
        bool hasRoughTex = aiMat->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &roughTex) == AI_SUCCESS;

        if (hasRoughTex)
        {
            std::filesystem::path texPath = m_Path.parent_path() / roughTex.C_Str();
            if (!std::filesystem::exists(texPath) || roughTex.length == 0)
                LNE_WARN("Roughness texture not found for mat: {0}", aiMat->GetName().C_Str());
            else
            {
                if (roughTex != metalTex)
                {
                    SafePtr<Texture> texture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                    material->SetTexture("tRoughness", texture);
                }
                else
                    material->SetTexture("tRoughness", metalTexture);
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
                LNE_WARN("Normal map not found for mat: {0}", aiMat->GetName().C_Str());
            else
            {
                SafePtr<Texture> texture = renderer.CreateTexture(texPath.string(), vk::Format::eR8G8B8A8Unorm);
                material->SetTexture("tNormal", texture);
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

void StaticMesh::GenerateUVSphereData(uint32_t nLatitude, uint32_t nLongitude, float radius, Vertex* oVertices, uint32_t* oIndices, uint32_t nVertices)
{
    float latitudeSlope = glm::pi<float>() / (float)(nLatitude + 1);
    // here, longitude points should be mapped between -180 and 180 degrees (or -PI to PI).
    float longitudeSlope = (2.f * glm::pi<float>()) / (float)nLongitude;

    uint32_t count = 0;
    // add north pole
    for (uint32_t i = 1; i <= nLongitude; ++i)
    {
        oVertices[count].Position = { 0.0f, radius, 0.0f };
        oVertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 0.0f };
        oVertices[count].Normal = { 0.0f, 1.0f, 0.0f };
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

            oVertices[count].Position = { radius * point.x, radius * point.y, radius * point.z };
            oVertices[count].TexCoord = { 1 - (float)j / (float)nLongitude, (float)i / (float)(nLatitude + 1) };
            oVertices[count].Normal = glm::vec3(point);

            ++count;
        }
    }

    //add south pole
    for (uint32_t i = 1; i <= nLongitude; ++i)
    {
        oVertices[count].Position = { 0.0f, -radius, 0.0f };
        oVertices[count].TexCoord = { (float)i / ((float)nLongitude + 1.0f), 1.0f };
        oVertices[count].Normal = { 0.0f, -1.0f, 0.0f };
        ++count;
    }

    count = 0;
    //north pole indices
    for (uint32_t i = 0; i < nLongitude; ++i)
    {
        oIndices[count++] = i;
        oIndices[count++] = (nLongitude - 1) + i + 2;
        oIndices[count++] = (nLongitude - 1) + i + 1;
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

            oIndices[count++] = index[0];
            oIndices[count++] = index[1];
            oIndices[count++] = index[2];

            oIndices[count++] = index[0];
            oIndices[count++] = index[2];
            oIndices[count++] = index[3];
        }
    }

    //south pole indices
    const uint32_t southPoleIndex = nVertices - nLongitude;
    for (uint32_t i = 0; i < nLongitude; ++i)
    {
        oIndices[count++] = southPoleIndex + i;
        oIndices[count++] = southPoleIndex - (nLongitude + 1) + i;
        oIndices[count++] = southPoleIndex - (nLongitude + 1) + i + 1;
    }
}

void StaticMesh::SetMaterial(SafePtr<Material> mat, uint32_t index)
{
    bool isMeshlet = mat->GetTechnique()->GetShaderDomain() == ShaderDomain::eMeshlet && m_Geometry->GetType() == GeometryType::eMeshlet;
    bool isClassic = mat->GetTechnique()->GetShaderDomain() == ShaderDomain::eMesh && m_Geometry->GetType() == GeometryType::eClassic;
    LNE_ASSERT(
        isMeshlet || isClassic, "Invalid material/geometry combination");
    if (index >= m_Materials.size())
    {
        LNE_WARN("Material index out of bounds");
        return;
    }
    m_Materials[index] = mat;
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

    geometry->m_VertexGPUBuffer = renderer.CreateGeometryBuffer(verticesPtr, vertices.size() * sizeof(Vertex));
    geometry->m_IndexGPUBuffer = renderer.CreateGeometryBuffer(indicesPtr, indices.size() * sizeof(uint32_t));
    geometry->m_Vertices = verticesPtr;
    geometry->m_Indices = indicesPtr;
    geometry->m_VertexCount = (uint32_t)vertices.size();
    geometry->m_IndexCount = (uint32_t)indices.size();

    SafePtr ctx = renderer.GetGfxContext();
    geometry->InitDescSet(ctx.GetPtr(), ctx->GetStorageOnlyDescriptorSetLayout(2));

    SafePtr<StaticMesh> mesh = lnnew StaticMesh();
    mesh->m_Geometry.reset(geometry);
    mesh->m_SubMeshes = { { "Cube", 0, 0, geometry->m_VertexCount, geometry->m_IndexCount, 0, AABB{.Min = {-1,-1,-1}, .Max = {1,1,1} } } };
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
    std::memset(vertices, 0, nVertices * sizeof(Vertex));
    uint32_t* indices = lnnew uint32_t[nIndices];

    // here, latitude points should be mapped between -90 and 90 degrees (or -PI/2 to PI/2).
    // +1 to nLat because it wouldn't make sense otherwise.
    GenerateUVSphereData(nLatitude, nLongitude, radius, vertices, indices, nVertices);

    Renderer& renderer = ApplicationBase::GetRenderer();
    geometry->m_VertexGPUBuffer = renderer.CreateGeometryBuffer(vertices, nVertices * sizeof(Vertex));
    geometry->m_IndexGPUBuffer = renderer.CreateGeometryBuffer(indices, nIndices * sizeof(uint32_t));

    geometry->m_VertexCount = nVertices;
    geometry->m_IndexCount = nIndices;

    geometry->m_Vertices = vertices;
    geometry->m_Indices = indices;

    SafePtr ctx = renderer.GetGfxContext();
    geometry->InitDescSet(ctx.GetPtr(), ctx->GetStorageOnlyDescriptorSetLayout(2));
    
    SafePtr<StaticMesh> mesh = lnnew StaticMesh();
    mesh->m_Geometry.reset(geometry);
    mesh->m_SubMeshes = { { "UVSphere", 0, 0, geometry->m_VertexCount, geometry->m_IndexCount, 0, AABB{.Min = {-radius,-radius,-radius}, .Max = {radius,radius,radius} } } };
    return mesh;
}

lne::SafePtr<StaticMesh> StaticMesh::GenerateUVSphereMeshlets(float radius /*= 1.f*/, uint32_t nLatitude /*= 32*/, uint32_t nLongitude /*= 32*/)
{
    if (nLatitude < 1)
        nLatitude = 1;
    if (nLongitude < 3)
        nLongitude = 3;

    SafePtr<StaticMesh> mesh = lnnew StaticMesh();

    uint32_t nVertices = nLatitude * (nLongitude + 1) + (nLongitude * 2);
    uint32_t nIndices = 2 * 3 * nLongitude + 2 * 3 * (nLatitude - 1) * nLongitude;

    Vertex* vertices = lnnew Vertex[nVertices];
    std::memset(vertices, 0, nVertices * sizeof(Vertex));
    uint32_t* indices = lnnew uint32_t[nIndices];

    GenerateUVSphereData(nLatitude, nLongitude, radius, vertices, indices, nVertices);

    // numbers advised by nvidia https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/
    const size_t maxVertices = 64;
    const size_t maxTriangles = 126;
    const float coneWeight = 0.0f;

    size_t maxMeshlets = meshopt_buildMeshletsBound(nIndices, maxVertices, maxTriangles);
    std::vector<meshopt_Meshlet> meshlets(maxMeshlets);
    std::vector<uint32_t> meshletVertices(nIndices);
    std::vector<uint8_t> meshletTriangles(nIndices);

    size_t meshletCount = meshopt_buildMeshlets(
        meshlets.data(), meshletVertices.data(), meshletTriangles.data(),
        indices, nIndices, (float*)vertices, nVertices, sizeof(Vertex),
        maxVertices, maxTriangles, coneWeight
    );
    meshopt_Meshlet& lastMeshlet = meshlets[meshletCount - 1];
    meshletVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
    meshletTriangles.resize(lastMeshlet.triangle_offset + lastMeshlet.triangle_count * 3);

    // could use meshopt_optimizeMeshlet later but for now, we test.
    Renderer& renderer = ApplicationBase::GetRenderer();
    SafePtr vertexBuffer = renderer.CreateGeometryBuffer(vertices, nVertices * sizeof(Vertex));

    void* meshletsData = new meshopt_Meshlet[meshletCount];
    std::memcpy(meshletsData, meshlets.data(), meshletCount * sizeof(meshopt_Meshlet));
    SafePtr meshletBuffer = renderer.CreateGeometryBuffer(meshletsData, meshletCount * sizeof(meshopt_Meshlet));

    void* meshletVerticesData = new uint32_t[meshletVertices.size()];
    std::memcpy(meshletVerticesData, meshletVertices.data(), meshletVertices.size() * sizeof(uint32_t));
    SafePtr meshletVertexIndicesBuffer = renderer.CreateGeometryBuffer(meshletVerticesData, meshletVertices.size() * sizeof(uint32_t));

    void* meshletTrianglesData = new uint8_t[meshletTriangles.size()];
    std::memcpy(meshletTrianglesData, meshletTriangles.data(), meshletTriangles.size() * sizeof(uint8_t));
    SafePtr meshletTriangleIndicesBuffer = renderer.CreateGeometryBuffer(meshletTrianglesData, meshletTriangles.size() * sizeof(uint8_t));
    Geometry* geometry;

    geometry = lnnew Geometry(
        renderer.GetGfxContext().GetPtr(),
        vertexBuffer,
        nullptr,
        meshletBuffer,
        meshletVertexIndicesBuffer,
        meshletTriangleIndicesBuffer,
        meshletsData,
        meshletVerticesData,
        meshletTrianglesData,
        vertices,
        nVertices,
        static_cast<uint32_t>(meshletCount)
    );

    mesh->m_Geometry.reset(geometry);
    mesh->m_SubMeshes = { { "UVSphere_Meshlets", 0, 0, geometry->m_VertexCount, geometry->m_IndexCount, 0, AABB{.Min = {-radius,-radius,-radius}, .Max = {radius,radius,radius} } } };
    delete[] indices;
    return mesh;
}

lne::SafePtr<lne::StaticMesh> StaticMesh::Clone() const
{
    SafePtr<StaticMesh> clone = lnnew StaticMesh();
    clone->m_Path = m_Path;

    clone->m_SubMeshes = m_SubMeshes;
    clone->m_Geometry = m_Geometry;
    clone->m_TotalVertexCount = m_TotalVertexCount;
    clone->m_TotalIndexCount = m_TotalIndexCount;

    clone->m_Materials = m_Materials;
    return clone;
}

Geometry::Geometry(GfxContext* ctx, SafePtr<StorageBuffer> vertexGPUBuffer, SafePtr<StorageBuffer> indexGPUBuffer, void* vertices, void* indices, uint32_t vertexCount, uint32_t indexCount)
    : m_Type(GeometryType::eClassic), m_VertexGPUBuffer(vertexGPUBuffer), m_IndexGPUBuffer(indexGPUBuffer),
      m_Vertices(vertices), m_Indices(indices), 
      m_VertexCount(vertexCount), m_IndexCount(indexCount)
{
    InitDescSet(ctx, ctx->GetStorageOnlyDescriptorSetLayout(2));
}

Geometry::Geometry(Geometry&& other) noexcept
    : m_VertexGPUBuffer(std::move(other.m_VertexGPUBuffer)),
    m_IndexGPUBuffer(std::move(other.m_IndexGPUBuffer)),
    m_Vertices(other.m_Vertices),
    m_Indices(other.m_Indices),
    m_VertexCount(other.m_VertexCount),
    m_IndexCount(other.m_IndexCount),
    m_DescSet(other.m_DescSet)
{
    other.m_Vertices = nullptr;
    other.m_Indices = nullptr;
    other.m_IndexCount = 0;
    other.m_VertexCount = 0;
    other.m_DescSet = nullptr;
}

Geometry::Geometry(GfxContext* ctx, 
                   SafePtr<StorageBuffer> vertexGPUBuffer, SafePtr<StorageBuffer> indexGPUBuffer, 
                   SafePtr<StorageBuffer> meshletGPUBuffer, 
                   SafePtr<StorageBuffer> meshletVertexIndicesGPUBuffer, 
                   SafePtr<StorageBuffer> meshletTriangleIndicesGPUBuffer, 
                   void* meshlets, void* meshletVertexIndices, void* meshletTriangleIndices,
                   void* vertices, uint32_t vertexCount, uint32_t meshletCount)
    : m_Type(GeometryType::eMeshlet), m_VertexGPUBuffer(vertexGPUBuffer), m_IndexGPUBuffer(indexGPUBuffer),
      m_MeshletGPUBuffer(meshletGPUBuffer), m_MeshletVertexIndicesGPUBuffer(meshletVertexIndicesGPUBuffer),
      m_MeshletTriangleIndicesGPUBuffer(meshletTriangleIndicesGPUBuffer),
      m_Meshlets(meshlets), m_MeshletVertexIndices(meshletVertexIndices), m_MeshletTriangleIndices(meshletTriangleIndices),
      m_Vertices(vertices), m_VertexCount(vertexCount), m_MeshletCount(meshletCount)
{
    InitDescSet(ctx, ctx->GetStorageOnlyDescriptorSetLayout(4));
}

Geometry& Geometry::operator=(Geometry&& other) noexcept
{
    if (this == &other)
        return *this;
    m_VertexGPUBuffer = std::move(other.m_VertexGPUBuffer);
    m_IndexGPUBuffer = std::move(other.m_IndexGPUBuffer);
    m_Vertices = other.m_Vertices;
    m_Indices = other.m_Indices;
    m_VertexCount = other.m_VertexCount;
    m_IndexCount = other.m_IndexCount;
    other.m_Vertices = nullptr;
    other.m_Indices = nullptr;
    other.m_IndexCount = 0;
    other.m_VertexCount = 0;
    return *this;
}

Geometry::~Geometry()
{
    delete[] m_Vertices;

    if (m_Type == GeometryType::eClassic)
        delete[] m_Indices;
    else if (m_Type == GeometryType::eMeshlet)
    {
        delete[] m_Meshlets;
        delete[] m_MeshletVertexIndices;
        delete[] m_MeshletTriangleIndices;
    }

    m_VertexGPUBuffer.Reset();
    m_IndexGPUBuffer.Reset();
    if (m_DescSet)
    {
        DescriptorSetDeletion resourceDeletion{
            .Type = DescriptorType::eStorageOnly,
            .DescriptorSet = m_DescSet,
        };
        ApplicationBase::GetRenderer().GetGfxContext()->EnqueueResourceDeletion(ResourceDeletion{
            .Type = ResourceType::eDescriptorSet,
            .Resource = resourceDeletion
        });
    }
}

void Geometry::InitDescSet(GfxContext* ctx, vk::DescriptorSetLayout layout)
{
    m_DescSet = ctx->AllocateDescriptorSet(layout, DescriptorType::eStorageOnly);
    std::vector<vk::WriteDescriptorSet> writeDescSets;
    switch (m_Type)
    {
    case GeometryType::eClassic:
    {
        vk::DescriptorBufferInfo vertexInfo = m_VertexGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo indexInfo = m_IndexGPUBuffer->GetDescriptorInfo();
        writeDescSets = {
            vk::WriteDescriptorSet{
                m_DescSet,
                0,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                & vertexInfo,
                nullptr
            },
            vk::WriteDescriptorSet{
                m_DescSet,
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
        break;
    }
    case GeometryType::eMeshlet:
    {
        vk::DescriptorBufferInfo vertexInfo = m_VertexGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo meshletInfo = m_MeshletGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo meshletVertexIndicesInfo = m_MeshletVertexIndicesGPUBuffer->GetDescriptorInfo();
        vk::DescriptorBufferInfo meshletTriangleIndicesInfo = m_MeshletTriangleIndicesGPUBuffer->GetDescriptorInfo();

        writeDescSets = {
            vk::WriteDescriptorSet{
                m_DescSet,
                0,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &vertexInfo,
                nullptr
            },
            vk::WriteDescriptorSet{
                m_DescSet,
                1,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &meshletInfo,
                nullptr
            },
            vk::WriteDescriptorSet{
                m_DescSet,
                2,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &meshletVertexIndicesInfo,
                nullptr
            },
            vk::WriteDescriptorSet{
                m_DescSet,
                3,
                0,
                1,
                vk::DescriptorType::eStorageBuffer,
                nullptr,
                &meshletTriangleIndicesInfo,
                nullptr
            }
        };
        ctx->GetDevice().updateDescriptorSets(writeDescSets, {});
        break;
    }
    }
}

}
