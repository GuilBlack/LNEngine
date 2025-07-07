//#lne_head [Vt main][Rp DepthPrePass]
#version 460

#extension GL_EXT_scalar_block_layout :     enable
#extension GL_EXT_nonuniform_qualifier :    require

layout(scalar, set=0, binding=0) uniform GlobalUBO {
    mat4 uViewProj;
    mat4 uView;
    mat4 uProj;
    vec3 uEyePos;
    vec3 uSunDir;
    float uAmbientLight;
};

layout(scalar, set = 1, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
} transformBuffer;

layout(scalar, set = 3, binding = 0) uniform MaterialData {
    float uDummy;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];

layout(set = 4, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];

#ifdef VERT

struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

layout(scalar, set = 2, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = 2, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main() {
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    mat4 model = transformBuffer.transforms[gl_InstanceIndex];
    gl_Position = uViewProj * model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0);
}

#endif
