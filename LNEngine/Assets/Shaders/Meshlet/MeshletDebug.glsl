//#lne_head [Ts main][Ms main][Fg main][Rp MeshletDebugPass][Tp Meshlet]
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shading_language_include : require
#extension GL_EXT_mesh_shader : require
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require

#include "Common.glslh"
#include "CommonMeshlet.glslh"

layout(scalar, set = TRANSFORM_SET, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
};

layout(scalar, push_constant) uniform PushConstants
{
    uint matId;
    uint instancesOffset;
};

struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

struct MaterialData {
    uint dummy;
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
    vec3 color;
    vec3 worldPos;
    vec3 normal;
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
        oMeshlet[gl_LocalInvocationIndex].worldPos = vec3(vertexBuffer.vertices[vertexIndex].position);
        oMeshlet[gl_LocalInvocationIndex].normal = vertexBuffer.vertices[vertexIndex].normal;
        gl_MeshVerticesEXT[gl_LocalInvocationIndex].gl_Position = uViewProj * transforms[instancesOffset] * vec4(vertexBuffer.vertices[vertexIndex].position, 1.0);
        oMeshlet[gl_LocalInvocationIndex].color = vec3(float(meshletIndex & 1), float(meshletIndex & 3) / 4.0, float(meshletIndex & 7) / 8.0);
    }
}

#endif

#ifdef FRAG

layout(location = 0) in Interpolants
{
    vec3 color;
    vec3 worldPos;
    vec3 normal;
} iMeshlet;

layout(location = 0) out vec4 oAlbedo;

void main()
{
    oAlbedo = vec4(iMeshlet.color, 1.0);
}

#endif
