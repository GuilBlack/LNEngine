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
    float uCubeSphereCoef;
    uint tDiffuse;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

#ifdef VERT

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec3 oUVW;

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
    Vertex v = vertexBuffer.vertices[currentIndex];
    vec3 normalizedPos = normalize(v.position.xyz);
    oUVW = normalizedPos;
    oUVW.y = -oUVW.y;

    vec3 finalPos = mix(v.position.xyz, normalizedPos, uCubeSphereCoef);

    gl_Position = uViewProj * uModel * vec4(finalPos, 1.0);
    oColor = vec4(vertexBuffer.vertices[currentIndex].color);
}

#endif

#ifdef FRAG

layout(location = 0) in vec4 iColor;
layout(location = 1) in vec3 iUVW;

layout(location = 0) out vec4 oColor;

void main() {
  oColor = uColor * texture(globalCubemaps[nonuniformEXT(tDiffuse)], iUVW);
}

#endif

