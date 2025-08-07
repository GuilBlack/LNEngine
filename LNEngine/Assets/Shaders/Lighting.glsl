//#lne_head [Vt main][Fg main][Rp LightingPass]
#version 460

#extension GL_EXT_scalar_block_layout :     enable
#extension GL_EXT_nonuniform_qualifier :    require

layout(scalar, set=0, binding=0) uniform GlobalUBO {
    mat4    uViewProj;
    mat4    uView;
    mat4    uProj;
    vec3    uEyePos;
    vec3    uSunDir;
    float   uAmbientLight;
    uint    tBRDFLut;
    uint    tIrradianceMap;
    uint    tPrefilteredMap;
};

layout(scalar, set = 2, binding = 0) uniform MaterialData {
    // texture indices
    uint tAlbedo;
    uint tNormal;
    uint tPosition;
    uint tMetalnessRoughness;
};

layout(set = 3, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 3, binding = 0) uniform samplerCube    globalCubemaps[];

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

// Schlick's approximation for the Fresnel Function
vec3 FresnelSchlick(float vDotH, vec3 F0) {
    return mix(F0,vec3(1),pow(1-vDotH,5));
}

// GGX Normal Distribution Function
float TrowbridgeReitzNDF(float nDotH, float alpha) {
    float a2 = alpha * alpha;
    float d = (nDotH * nDotH) * (a2 - 1) + 1;
    return a2 / (PI * d * d);
}

// Schlick-GGX by Schlick & Beckman Geometry Shadowing Function
float SchlickBeckmanGSF(float nDotL, float nDotV, float alpha) {
    float r = (alpha + 1.0);
    float k = (r * r) / 8.0;

    float gL = nDotL / (nDotL * (1.0 - k) + k);
    float gV = nDotV / (nDotV * (1.0 - k) + k);

    return gL * gV;
}

vec3 samplePrefilteredReflection(vec3 reflectDir, float roughness) {
    float maxReflLod = log2(float(textureSize(globalTextures[nonuniformEXT(tPrefilteredMap)], 0).x));
    float lod = maxReflLod * roughness;
    float lodMin = floor(lod);
    float lodMax = ceil(lod);
    vec3 sample1 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMin).xyz;
    vec3 sample2 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMax).xyz;
    return mix(sample1, sample2, lod - lodMin);
}

void main()
{
    vec3 albedo = texture(globalTextures[nonuniformEXT(tAlbedo)], iUV).xyz;
    vec3 normal = normalize(texture(globalTextures[nonuniformEXT(tNormal)], iUV).xyz);
    vec3 position = texture(globalTextures[nonuniformEXT(tPosition)], iUV).xyz;
    vec3 metalnessRoughness = texture(globalTextures[nonuniformEXT(tMetalnessRoughness)], iUV).xyz;
    float metalness = metalnessRoughness.x;
    float roughness = metalnessRoughness.y;

    vec3 viewDir = normalize(uEyePos - position);
    vec3 lightDir = normalize(-uSunDir);
    vec3 lightColor = length(uSunDir) * vec3(1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    vec3 reflectDir = reflect(-viewDir, normal);

    float nDotL = max(0.0, dot(normal, lightDir));
    float nDotV = max(0.0, dot(normal, viewDir));
    float nDotH = max(0.0, dot(normal, halfDir));
    float vDotH = max(0.0, dot(viewDir, halfDir));

    // Calculate FresnelSchlick
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    vec3 F = FresnelSchlick(vDotH, F0);

    // Calculate Cook-Torrance dielectric ratio
    vec3 kD = (1.0 - F) * (1.0 - metalness);

    // --- direct lighting ---
    float alpha = roughness * roughness;
    float denom = 4.0 * nDotL * nDotV + 1e-5; // prevent division by zero
    vec3 specSun = (TrowbridgeReitzNDF(nDotH, alpha) * SchlickBeckmanGSF(nDotL, nDotV, alpha) * F) / denom;
    vec3  diffuseSun  = (kD * albedo) / PI;
    vec3 directLight = (diffuseSun + specSun) * nDotL * lightColor;

    // --- IBL ---
    vec2 brdf = texture(globalTextures[nonuniformEXT(tBRDFLut)], vec2(nDotV, roughness)).xy;
    vec3 prefilteredColor = samplePrefilteredReflection(reflectDir, roughness);
    vec3 irradiance = texture(globalCubemaps[nonuniformEXT(tIrradianceMap)], normal).xyz;
    vec3 diffuseIBL = kD * (irradiance * albedo) / PI;
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 color = uAmbientLight * albedo + diffuseIBL + specularIBL + directLight;

    oColor = vec4(color, 1.0);
}

#endif
