//#lne_head [Vt main][Fg main][Rp LightingPass]
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

layout(scalar, set = 2, binding = 0) uniform MaterialData {
    // texture indices
    uint tAlbedo;
    uint tNormal;
    uint tPosition;
};

layout(set = 3, binding = 0) uniform sampler2D      globalTextures[];

layout(set = 3, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];

const float PI = 3.14159265359;
const float TWO_OVER_PI = 2.0 / PI;

#ifdef VERT

struct Vertex {
    vec3 position;
    vec2 uv;
};

layout(scalar, set = 1, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = 1, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

layout(location = 0) out vec2 oUV;

void main()
{
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    Vertex v = vertexBuffer.vertices[currentIndex];
    
    // fullscreen quad
    gl_Position = vec4(v.position, 1.0);
    oUV = v.uv;
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUV;

layout(location = 0) out vec4 oColor;

void main()
{
    vec4 albedo = texture(globalTextures[nonuniformEXT(tAlbedo)], iUV);
    vec4 normal = texture(globalTextures[nonuniformEXT(tNormal)], iUV);
    vec4 position = texture(globalTextures[nonuniformEXT(tPosition)], iUV);
    
    oColor = vec4(albedo.xyz, 1.0);
}

#endif
