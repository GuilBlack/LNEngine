//#lne_head [Vt main][Fg main][Rp LightingPass][Tp PostProcess]
#version 460

#include "Common.glslh"
#include "CommonPostProcess.glslh"

struct MaterialData {
    // texture indices
    uint tAlbedo;
    uint tNormal;
    uint tPosition;
    uint tMetalnessRoughness;
};

layout(scalar, push_constant) uniform MatPC {
    uint id;
} matPC;

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} mb;

layout(set = TEX_SET, binding = 0) uniform sampler2D      globalTextures[];
layout(set = TEX_SET, binding = 0) uniform samplerCube    globalCubemaps[];

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
    
    // fullscreen quad
    gl_Position = vec4(v.position, 1.0);
    oUV = v.uv;
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUV;

layout(location = 0) out vec4 oColor;

#include "PBRLighting.glslh"

vec3 samplePrefilteredReflection(vec3 reflectDir, float roughness) {
    float maxReflLod = log2(float(textureSize(globalTextures[nonuniformEXT(tPrefilteredMap)], 0).x));
    float lod = maxReflLod * roughness;
    float lodMin = floor(lod);
    float lodMax = ceil(lod);
    vec3 sample1 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMin).xyz;
    vec3 sample2 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMax).xyz;
    return mix(sample1, sample2, lod - lodMin);
}

void main() {
    MaterialData mat = mb.materials[matPC.id];
    vec3 albedo = texture(globalTextures[nonuniformEXT(mat.tAlbedo)], iUV).xyz;
    vec3 normal = normalize(texture(globalTextures[nonuniformEXT(mat.tNormal)], iUV).xyz);
    vec3 position = texture(globalTextures[nonuniformEXT(mat.tPosition)], iUV).xyz;
    vec3 metalnessRoughness = texture(globalTextures[nonuniformEXT(mat.tMetalnessRoughness)], iUV).xyz;
    float metalness = metalnessRoughness.x;
    float roughness = metalnessRoughness.y;

    vec3 viewDir = normalize(uEyePos - position);
    vec3 reflectDir = reflect(-viewDir, normal);
    float nDotV = max(0.0, dot(normal, viewDir));

    // --- IBL ---
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    vec3 F = FresnelSchlick(nDotV, F0);

    // Calculate Cook-Torrance dielectric ratio
    vec3 kD = (1.0 - F) * (1.0 - metalness);

    vec2 brdf = texture(globalTextures[nonuniformEXT(tBRDFLut)], vec2(nDotV, roughness)).xy;
    vec3 prefilteredColor = samplePrefilteredReflection(reflectDir, roughness);
    vec3 irradiance = texture(globalCubemaps[nonuniformEXT(tIrradianceMap)], normal).xyz;
    vec3 diffuseIBL = kD * (irradiance * albedo) * INV_PI;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    vec3 iblLight = diffuseIBL + specularIBL;

    ComputeAllLightingArgs lightingArgs;
    lightingArgs.normal = normal;
    lightingArgs.position = position;
    lightingArgs.viewDir = viewDir;
    lightingArgs.albedo = albedo;
    lightingArgs.metalness = metalness;
    lightingArgs.roughness = roughness;

    vec3 color = ComputeLighting(lightingArgs) + iblLight + uAmbientLight;

    oColor = vec4(color, 1.0);
}

#endif
