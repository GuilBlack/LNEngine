//#lne_head [Vt main][Fg main][Rp TransparentForwardPass][Tp Mesh]
#version 460

#include "Common.glslh"
#include "CommonMesh.glslh"

layout(scalar, set = TRANSFORM_SET, binding = 0) readonly buffer TransformBuffer {
    mat4 transforms[];
} transformBuffer;

struct MaterialData {
    vec4 uColor;
    float uMetalness;
    float uRoughness;

    // texture indices
    uint tAlbedo;
    uint tMetalness;
    uint tRoughness;
    uint tNormal;
};

layout(scalar, push_constant) uniform MatPC {
    uint id;
} matPC;

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[]; // MaterialData
} mb;

layout(set = TEX_SET, binding = 0) uniform sampler2D      globalTextures[];
layout(set = TEX_SET, binding = 0) uniform samplerCube    globalCubemaps[];
layout(set = TEX_SET, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = TEX_SET, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = TEX_SET, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];

#ifdef VERT

layout(location = 0) out vec2 oUVs;
layout(location = 1) out vec3 oWorldPos;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec3 oTangent;
layout(location = 4) out vec3 oBitangent;

struct Vertex {
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

layout(scalar, set = VERTEX_SET, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
} vertexBuffer;

layout(set = VERTEX_SET, binding = 1) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

void main()
{
    uint currentIndex = indexBuffer.indices[gl_VertexIndex];
    mat4 model = transformBuffer.transforms[gl_InstanceIndex];
    gl_Position = uViewProj * model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0);
    oUVs = vertexBuffer.vertices[currentIndex].uv;

    oWorldPos = (model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    oNormal = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].normal);
    if (vertexBuffer.vertices[currentIndex].tangent.xyz == vec3(0.0))
    {
        oTangent = vec3(0.0);
        oBitangent = vec3(0.0);
    }
    else
    {
        oTangent = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].tangent.xyz);
        oBitangent = vertexBuffer.vertices[currentIndex].tangent.w * cross(oNormal, oTangent); // tangent.w = normal space handedness
    }
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUVs;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec3 iTangent;
layout(location = 4) in vec3 iBitangent;

layout(location = 0) out vec4 oColor;

#include "PBR.glslh"

vec3 samplePrefilteredReflection(vec3 reflectDir, float roughness)
{
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
    MaterialData mat = mb.materials[matPC.id];
    vec4 albedoMapValue = texture(globalTextures[mat.tAlbedo], iUVs);
    vec3 albedo = albedoMapValue.xyz;

    if (albedoMapValue.w == 0.0)
        discard;

    float metalness = mat.uMetalness;
    if (mat.tMetalness != 0)
        metalness = texture(globalTextures[nonuniformEXT(mat.tMetalness)], iUVs).z;
    float roughness = mat.uRoughness;
    if (mat.tRoughness != 0)
        roughness = texture(globalTextures[nonuniformEXT(mat.tRoughness)], iUVs).y;
        
    vec3 normal = normalize(iNormal);
    if (mat.tNormal != 0 && iTangent != vec3(0.0))
    {
        vec3 tangent =   normalize(iTangent - normal * dot(normal, iTangent));
        vec3 bitangent = normalize(iBitangent);

        mat3 TBN = mat3(tangent, bitangent, normal);
        normal = vec4(TBN * (texture(globalTextures[nonuniformEXT(mat.tNormal)], iUVs).xyz * 2.0 - vec3(1.0)), 1.0).xyz;
    }

    vec3 viewDir = normalize(uEyePos - iWorldPos);
    vec3 lightDir = normalize(-uSunDir);
    vec3 lightColor = length(uSunDir) * vec3(1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    vec3 reflectDir = reflect(-viewDir, normal);

    float nDotL = max(0.0, dot(normal, lightDir));
    float nDotV = max(0.0, dot(normal, viewDir));
    float nDotH = max(0.0, dot(normal, halfDir));
    float vDotH = max(0.0, dot(viewDir, halfDir));

    // Sample the prefiltered environment map

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
