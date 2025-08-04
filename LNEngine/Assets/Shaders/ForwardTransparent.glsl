//#lne_head [Vt main][Fg main][Rp TransparentForwardPass]
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
    uint tNormal;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

layout(set = 4, binding = 1, rgba8) uniform writeonly image2D   globalImageRgba8[];
layout(set = 4, binding = 1, rgba16f) uniform writeonly image2D globalImageRgba16f[];
layout(set = 4, binding = 1, rgba32f) uniform writeonly image2D globalImageRgba32f[];


const float PI = 3.14159265359;
const float TWO_OVER_PI = 2.0 / PI;

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
    oUVs = vertexBuffer.vertices[currentIndex].uv;

    oWorldPos = (model * vec4(vertexBuffer.vertices[currentIndex].position, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    oNormal = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].normal);
    oTangent = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].tangent.xyz);
    oBitangent = vertexBuffer.vertices[currentIndex].tangent.w * cross(oNormal, oTangent); // tangent.w = normal space handedness
}

#endif

#ifdef FRAG

layout(location = 0) in vec2 iUVs;
layout(location = 1) in vec3 iWorldPos;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec3 iTangent;
layout(location = 4) in vec3 iBitangent;

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

void main() {
    vec4 albedoMapValue = texture(globalTextures[tAlbedo], iUVs);
    vec3 albedo = albedoMapValue.xyz;

    if (albedoMapValue.w == 0.0)
        discard;

    float metalness = uMetalness;
    if (tMetalness != 0)
        metalness = texture(globalTextures[nonuniformEXT(tMetalness)], iUVs).z;
    float roughness = uRoughness;
    if (tRoughness != 0)
        roughness = texture(globalTextures[nonuniformEXT(tRoughness)], iUVs).y;
        
    vec3 normal = normalize(iNormal);
    if (tNormal != 0)
    {
        vec3 tangent =   normalize(iTangent - normal * dot(normal, iTangent));
        vec3 bitangent = normalize(iBitangent);

        mat3 TBN = mat3(tangent, bitangent, normal);
        normal = vec3(TBN * (texture(globalTextures[nonuniformEXT(tNormal)], iUVs).xyz * 2.0 - vec3(1.0)));
    }
    vec3 viewDir = normalize(uEyePos - iWorldPos);
    vec3 lightDir = normalize(-uSunDir);
    vec3 halfDir = normalize(lightDir + viewDir);

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

    // lambert diffuse
    vec3 diffuse = kD * albedo / PI;

    // Cook-Torrance microfacet specular
    float alpha = roughness * roughness;
    float denom = 4.0 * nDotL * nDotV + 0.0001;
    vec3 DFG = TrowbridgeReitzNDF(nDotH, alpha) * SchlickBeckmanGSF(nDotL, nDotV, alpha) * F;

    vec3 specular = DFG / denom;

    vec3 ambientCol = uAmbientLight * albedo;
    vec3 color = ambientCol + nDotL * (diffuse + specular);

    oColor = vec4(color, 1.0);
}

#endif
