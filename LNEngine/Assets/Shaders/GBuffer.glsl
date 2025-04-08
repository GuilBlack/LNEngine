//#lne_head [Vt main][Fg main][Rp GBufferPass]
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
    vec4 uColor;
    float uMetalness;
    float uRoughness;

    // texture indices
    uint tAlbedo;
    uint tMetalness;
    uint tRoughness;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

layout(set = 4, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = 4, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = 4, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];


const float PI = 3.14159265359;
const float TWO_OVER_PI = 2.0 / PI;

#ifdef VERT

layout(location = 0) out vec2 oUV;
layout(location = 1) out vec3 oNormal;
layout(location = 2) out vec3 oWorldPos;

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

void main()
{
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    mat4 model = transformBuffer.transforms[gl_InstanceIndex];
    gl_Position = uViewProj * model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0);
    oUV = vertexBuffer.vertices[currentIndex].uv;

    oWorldPos = (model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    oNormal = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].normal);
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUV;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec3 iWorldPos;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oMetalnessRoughness;

void main()
{
    oAlbedo = texture(globalTextures[nonuniformEXT(tAlbedo)], iUV);
    oNormal = vec4(iNormal, 1.0);
    oPosition = vec4(iWorldPos, 1.0);
    
    float metalness = uMetalness;
//    if (tMetalness != 0)
//        metalness = texture(globalTextures[nonuniformEXT(tMetalness)], iUV).z;
    float roughness = uRoughness;
//    if (tRoughness != 0)
//        roughness = texture(globalTextures[nonuniformEXT(tRoughness)], iUV).y;
    oMetalnessRoughness = vec4(metalness, roughness, 0.0, 1.0);
}

#endif