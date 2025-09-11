//#lne_head [Vt main][Fg main][Rp GBufferPass][Tp Mesh]
#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shading_language_include : require

#include "Common.glslh"
#include "CommonMesh.glslh"

layout(scalar, set = TRANSFORM_SET, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
} transformBuffer;

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

layout(scalar, push_constant) uniform MatPC
{
    uint id;
} matPC;

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[]; // MaterialData
} mb;

layout(set = TEX_SET, binding = 0) uniform sampler2D      globalTextures[];
layout(set = TEX_SET, binding = 0) uniform samplerCube    globalCubemaps[];

layout(set = TEX_SET, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = TEX_SET, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = TEX_SET, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];

#ifdef VERT

layout(location = 0) out vec2 oUV;
layout(location = 1) out vec3 oWorldPos;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec3 oTangent;
layout(location = 4) out vec3 oBitangent;

struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

layout(scalar, set = VERTEX_SET, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = VERTEX_SET, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main()
{
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    mat4 model = transformBuffer.transforms[gl_InstanceIndex];
    gl_Position = uViewProj * model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0);
    oUV = vertexBuffer.vertices[currentIndex].uv;

    oWorldPos = (model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    oNormal = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].normal);
    oTangent = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].tangent.xyz);
    oBitangent = vertexBuffer.vertices[currentIndex].tangent.w * cross(oNormal, oTangent); // tangent.w = normal space handedness
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUV;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec3 iTangent;
layout(location = 4) in vec3 iBitangent;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oMetalnessRoughness;

void main()
{
    MaterialData mat = mb.materials[matPC.id];
    oAlbedo = texture(globalTextures[nonuniformEXT(mat.tAlbedo)], iUV);
    oPosition = vec4(iWorldPos, 1.0);

    float metalness = mat.uMetalness;
    if (mat.tMetalness != 0)
        metalness = texture(globalTextures[nonuniformEXT(mat.tMetalness)], iUV).z;
    float roughness = mat.uRoughness;
    if (mat.tRoughness != 0)
        roughness = texture(globalTextures[nonuniformEXT(mat.tRoughness)], iUV).y;
    oMetalnessRoughness = vec4(metalness, roughness, 0.0, 1.0);

    vec3 normal = normalize(iNormal);
    if (mat.tNormal != 0)
    {
        vec3 tangent =   normalize(iTangent - normal * dot(normal, iTangent));
        vec3 bitangent = normalize(iBitangent);

        mat3 TBN = mat3(tangent, bitangent, normal);
        oNormal = vec4(TBN * (texture(globalTextures[nonuniformEXT(mat.tNormal)], iUV).xyz * 2.0 - vec3(1.0)), 1.0);
    }
    else
        oNormal = vec4(normal, 1.0);
}

#endif
