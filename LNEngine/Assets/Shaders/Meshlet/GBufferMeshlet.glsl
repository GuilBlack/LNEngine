//#lne_head [Ts main][Ms main][Fg main][Rp GBufferPass][Tp Meshlet]
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shading_language_include : require
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require

#include "Common.glslh"
#include "CommonMeshlet.glslh"

layout(scalar, set = TRANSFORM_SET, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
};

struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

struct MaterialData
{
    vec4 uColor;
    float uMetalness;
    float uRoughness;

    // texture indices
    uint tAlbedo;
    uint tMetalness;
    uint tRoughness;
    uint tNormal;
};

layout(scalar, push_constant) uniform PushConstants
{
    uint matId;
    uint instancesOffset;
};

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[]; // MaterialData
} mb;

layout(set = TEX_SET, binding = 0) uniform sampler2D      globalTextures[];
layout(set = TEX_SET, binding = 0) uniform samplerCube    globalCubemaps[];

layout(set = TEX_SET, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = TEX_SET, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = TEX_SET, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];

#if defined(TASK) || defined(MESH)

layout(scalar, set = MESH_SET, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;
layout(scalar, set = MESH_SET, binding = 1) readonly buffer MeshletBuffer {
    Meshlet meshlets[];
} meshletBuffer;
layout(scalar, set = MESH_SET, binding = 2) readonly buffer VertexIndicesBuffer {
    uint vertexIndices[];
} vertexIndicesBuffer;
layout(scalar, set = MESH_SET, binding = 3) readonly buffer TriangleIndicesBuffer {
    uint8_t triangleIndices[];
} triangleIndicesBuffer;

const uint WORKGROUP_SIZE = 32;

struct Payload
{
    uint meshletIndices[WORKGROUP_SIZE];
};

taskPayloadSharedEXT Payload sPayload;

#endif

#ifdef TASK

layout(local_size_x = WORKGROUP_SIZE) in;

void main()
{
    uint liIdx = gl_LocalInvocationID.x;
    uint giIdx = gl_GlobalInvocationID.x;
    mat4 model = transforms[instancesOffset];
    Meshlet meshlet = meshletBuffer.meshlets[giIdx];

    vec4 center = model * vec4(meshlet.BoundsCenter, 1.0);
    float radius = meshlet.BoundsRadius * max(
        length(vec3(model[0][0], model[1][0], model[2][0])),
        max(
            length(vec3(model[0][1], model[1][1], model[2][1])),
            length(vec3(model[0][2], model[1][2], model[2][2]))
        ) * 1.05
    ); // scale the bounding sphere radius + a small bias

    vec3 coneAxis = vec3(
        int(meshlet.ConeAxis[0]) / 127.0,
        int(meshlet.ConeAxis[1]) / 127.0,
        int(meshlet.ConeAxis[2]) / 127.0
    );
    coneAxis = (transpose(inverse(mat3(model))) * coneAxis);

    float coneCutoff = float(int(meshlet.ConeCutoff)) / 127.0;
    bool visible = cullCone(coneAxis, coneCutoff, center.xyz, radius, uEyePos);
    uvec4 mask = subgroupBallot(visible);

    uint index = subgroupBallotExclusiveBitCount(mask);

    if (visible)
        sPayload.meshletIndices[index] = giIdx;

    uint totalVisible = subgroupBallotBitCount(mask);

    barrier();
    if (subgroupElect())
        EmitMeshTasksEXT(totalVisible, 1, 1);
}

#endif

#ifdef MESH

layout(local_size_x = 128) in;
layout(triangles, max_vertices = 64, max_primitives = 128) out;

layout(location = 0) out Interpolants
{
    vec3 worldPos;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
} oMeshlet[];

void main()
{
    uint meshletIndex = sPayload.meshletIndices[gl_WorkGroupID.x];
    Meshlet meshlet = meshletBuffer.meshlets[meshletIndex];
    SetMeshOutputsEXT(meshlet.VertexCount, meshlet.TriangleCount);

    // thread only works on assigned triangles
    if (gl_LocalInvocationIndex < meshlet.TriangleCount)
    {
        uint triByteOffset = meshlet.TriangleOffset + gl_LocalInvocationIndex * 3;
        uint8_t i0 = triangleIndicesBuffer.triangleIndices[triByteOffset + 0];
        uint8_t i1 = triangleIndicesBuffer.triangleIndices[triByteOffset + 1];
        uint8_t i2 = triangleIndicesBuffer.triangleIndices[triByteOffset + 2];
        gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex] = uvec3(uint(i0), uint(i1), uint(i2));
    }

    if (gl_LocalInvocationIndex < meshlet.VertexCount)
    {
        uint vertexIndex = meshlet.VertexOffset + gl_LocalInvocationIndex;
        vertexIndex = vertexIndicesBuffer.vertexIndices[vertexIndex];
        mat4 model = transforms[instancesOffset];

        vec4 worldPos = model * vec4(vertexBuffer.vertices[vertexIndex].position, 1.0);
        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = uViewProj * worldPos;
        oMeshlet[gl_LocalInvocationIndex].worldPos = worldPos.xyz;

        oMeshlet[gl_LocalInvocationIndex].uv = vertexBuffer.vertices[vertexIndex].uv;

        mat3 normalMatrix = transpose(inverse(mat3(model)));
        oMeshlet[gl_LocalInvocationIndex].normal = normalize(normalMatrix * vertexBuffer.vertices[vertexIndex].normal);
        if (vertexBuffer.vertices[vertexIndex].tangent.xyz == vec3(0.0))
        {
            oMeshlet[gl_LocalInvocationIndex].tangent = vec3(0.0);
            oMeshlet[gl_LocalInvocationIndex].bitangent = vec3(0.0);
        }
        else
        {
            oMeshlet[gl_LocalInvocationIndex].tangent = normalize(normalMatrix * vertexBuffer.vertices[vertexIndex].tangent.xyz);
            oMeshlet[gl_LocalInvocationIndex].bitangent = normalize(cross(oMeshlet[gl_LocalInvocationIndex].normal, oMeshlet[gl_LocalInvocationIndex].tangent) * vertexBuffer.vertices[vertexIndex].tangent.w);
        }
    }
}

#endif

#ifdef FRAG

layout(location = 0) in Interpolants
{
    vec3 worldPos;
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec2 uv;
} iMeshlet;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oMetalnessRoughness;

void main()
{
    MaterialData mat = mb.materials[matId];
    oAlbedo = mat.tAlbedo == 0 ? mat.uColor : texture(globalTextures[nonuniformEXT(mat.tAlbedo)], iMeshlet.uv);
    oPosition = vec4(iMeshlet.worldPos, 1.0);

    float metalness = mat.tMetalness == 0 ? mat.uMetalness : texture(globalTextures[nonuniformEXT(mat.tMetalness)], iMeshlet.uv).z;
    float roughness = mat.tRoughness == 0 ? mat.uRoughness : texture(globalTextures[nonuniformEXT(mat.tRoughness)], iMeshlet.uv).y;
    oMetalnessRoughness = vec4(metalness, roughness, 0.0, 1.0);

    vec3 normal = normalize(iMeshlet.normal);
    if (mat.tNormal != 0 && iMeshlet.tangent != vec3(0.0))
    {
        vec3 tangent =   normalize(iMeshlet.tangent - normal * dot(normal, iMeshlet.tangent));
        vec3 bitangent = normalize(iMeshlet.bitangent);

        mat3 TBN = mat3(tangent, bitangent, normal);
        oNormal = vec4(TBN * (texture(globalTextures[nonuniformEXT(mat.tNormal)], iMeshlet.uv).xyz * 2.0 - vec3(1.0)), 1.0);
    }
    else
        oNormal = vec4(normal, 1.0);
}

#endif
