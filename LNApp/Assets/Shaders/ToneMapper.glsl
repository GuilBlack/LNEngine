//#lne_head [Vt main][Fg main][Rp ToneMappingPass]
#version 460

#include "Common.glslh"
#include "CommonPostProcess.glslh"

layout(scalar, set = MAT_SET, binding = 0) uniform MaterialData {
    uint tSceneTexture;
};

layout(set = TEX_SET, binding = 0) uniform sampler2D                  globalTextures[];

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
    vec2 uv = iUV * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    vec3 mappedColor = vec3(1.0) - exp(-texture(globalTextures[nonuniformEXT(tSceneTexture)], uv).xyz);
    oColor = vec4(mappedColor, 0.0);
}

#endif
