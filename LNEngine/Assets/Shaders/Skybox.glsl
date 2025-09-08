//#lne_head [Vt main][Fg main][Rp SkyboxPass][Tp PostProcess]
#version 460

#include "Common.glslh"
#include "CommonPostProcess.glslh"

struct MaterialData {
    uint tCubeAlbedo;
};

layout(scalar, push_constant) uniform MatPC
{
    uint id;
} matPC;

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} mb;

layout(set = TEX_SET, binding = 0) uniform sampler2D                  globalTextures[];
layout(set = TEX_SET, binding = 0) uniform samplerCube                globalCubemaps[];

layout(set = TEX_SET, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];

#ifdef VERT

struct Vertex {
    vec3 position;
    vec2 uv;
};

layout(scalar, set = VERTEX_SET, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = VERTEX_SET, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

layout(location = 0) out vec2 oUV;

void main() {
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    Vertex v = vertexBuffer.vertices[currentIndex];
    
    gl_Position = vec4(v.position.xy, 1.0, 1.0);
    oUV = v.position.xy;
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUV;

layout(location = 0) out vec4 oColor;

void main() {
    vec4 clipSpacePos = vec4(iUV, 0.0, 1.0);
    
    // Convert clip space position to world space
    vec4 viewDir = inverse(uProj) * clipSpacePos;
    viewDir = vec4(viewDir.xy, -1.0, 0.0); 
    vec3 worldDir = normalize((inverse(uView) * viewDir).xyz);

    oColor = textureLod(globalCubemaps[nonuniformEXT(mb.materials[matPC.id].tCubeAlbedo)], worldDir, 0);
}

#endif
