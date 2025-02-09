//#lne_head [Vt main][Fg main][Rp BasicForwardPass]
#version 460

#extension GL_EXT_scalar_block_layout :     enable
#extension GL_EXT_nonuniform_qualifier :    require

layout(scalar, set=0, binding=0) uniform GlobalUBO {
    mat4 uViewProj;
    mat4 uView;
    mat4 uProj;
    vec3 uEyePos;
    vec3 uSunDir;
};

layout(scalar, set = 1, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
} transformBuffer;

layout(scalar, set = 3, binding = 0) uniform MaterialData {
    vec4 uColor;
    float uMetalness;
    float uRoughness;

    // texture indices
    uint tAlbedo;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

layout(set = 4, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = 4, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = 4, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];

#ifdef VERT

layout(location = 0) out vec3 oUVW;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

layout(scalar, set = 2, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = 2, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main() {
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    Vertex v = vertexBuffer.vertices[currentIndex];
    oUVW = v.position.xyz;
    oUVW.xy = -oUVW.xy;

    mat4 viewMat = mat4(mat3(uView));
    vec4 pos = uProj * viewMat * vec4(v.position.xyz, 1.0);
    gl_Position = pos.xyww;
}

#endif

#ifdef FRAG

layout (location = 0) in vec3 iUVW;

layout (location = 0) out vec4 oColor;

void main() {
    oColor = texture(globalCubemaps[nonuniformEXT(tAlbedo)], iUVW);
}

#endif
