//#lne_head [[Vt main][Fg main]]
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

layout(scalar, set = 2, binding = 0) uniform ObjectData {
    mat4 uModel;
};

layout(scalar, set = 3, binding = 0) uniform MaterialData {
    uint tDiffuse;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

#ifdef VERT

layout(location = 0) out vec3 oUVW;

struct Vertex {
    vec4 position;
    vec4 normal;
    vec2 uv;
    vec4 color;
};

layout(scalar, set = 1, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = 1, binding = 1) readonly buffer IndexBuffer {
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
    oColor = texture(globalCubemaps[nonuniformEXT(tDiffuse)], iUVW);
}

#endif
