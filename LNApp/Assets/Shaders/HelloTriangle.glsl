//#lne_head [[Vt main][Fg main]]
#version 460

#extension GL_EXT_scalar_block_layout :     enable
#extension GL_EXT_nonuniform_qualifier :    require

layout(scalar, set=0, binding=0) uniform GlobalUBO {
    mat4 uViewProj;
    mat4 uView;
    mat4 uProj;
};

layout(scalar, set = 2, binding = 0) uniform ObjectData {
    mat4 uModel;
};

layout(scalar, set = 3, binding = 0) uniform MaterialData {
    vec4 uColor;
    uint tDiffuse;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

#ifdef VERT

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec2 oUVs;

vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5));

vec3 colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

struct Vertex {
    vec4 position;
    vec4 color;
    vec2 uv;
};

layout(scalar, set = 1, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = 1, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main() {
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    gl_Position = uViewProj * uModel * vec4(vertexBuffer.vertices[currentIndex].position);
    oColor = vec4(vertexBuffer.vertices[currentIndex].color);
    oUVs = vertexBuffer.vertices[currentIndex].uv;
}

#endif

#ifdef FRAG

layout(location = 0) in vec4 iColor;
layout(location = 1) in vec2 iUVs;
layout(location = 0) out vec4 oColor;

void main() {
  oColor = uColor * texture(globalTextures[nonuniformEXT(tDiffuse)], iUVs);
}

#endif
