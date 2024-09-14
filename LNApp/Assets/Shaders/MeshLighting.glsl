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
    vec4 uColor;
    float uMetalness;
    float uRoughness;
    
    // texture indices
    uint tAlbedo;
};

layout(set = 4, binding = 0) uniform sampler2D      globalTextures[];
layout(set = 4, binding = 0) uniform samplerCube    globalCubemaps[];

const float PI = 3.14159265359;
const float TWO_OVER_PI = 2.0 / PI;

#ifdef VERT

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec2 oUVs;
layout(location = 2) out vec3 oNormal;
layout(location = 3) out vec3 oWorldPos;

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
    gl_Position = uViewProj * uModel * vec4(vertexBuffer.vertices[currentIndex].position);
    oColor = vec4(vertexBuffer.vertices[currentIndex].color);
    oUVs = vertexBuffer.vertices[currentIndex].uv;

    oWorldPos = (uModel * vertexBuffer.vertices[currentIndex].position).xyz;

    mat3 normalMatrix = inverse(mat3(uModel));
    oNormal = normalize(normalMatrix * vertexBuffer.vertices[currentIndex].normal.xyz);
}

#endif

#ifdef FRAG

layout(location = 0) in vec4 iColor;
layout(location = 1) in vec2 iUVs;
layout(location = 2) in vec3 iNormal;
layout(location = 3) in vec3 iWorldPos;

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
    vec3 albedo = texture(globalTextures[tAlbedo], iUVs).xyz;
    vec3 normal = normalize(iNormal);
    vec3 viewDir = normalize(uEyePos - iWorldPos);
    vec3 lightDir = normalize(uSunDir);
    vec3 halfDir = normalize(lightDir + viewDir);

    float nDotL = max(0.0, dot(normal, lightDir));
    float nDotV = max(0.0, dot(normal, viewDir));
    float nDotH = max(0.0, dot(normal, halfDir));
    float vDotH = max(0.0, dot(viewDir, halfDir));

    // Calculate FresnelSchlick
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, uMetalness);
    vec3 F = FresnelSchlick(vDotH, F0);

    // Calculate Cook-Torrance dielectric ratio
    vec3 kD = (1.0 - F) * (1.0 - uMetalness);

    // lambert diffuse
    vec3 diffuse = kD * albedo / PI;

    // Cook-Torrance microfacet specular
    float alpha = uRoughness * uRoughness;
    float denom = 4.0 * nDotL * nDotV + 0.0001;
    vec3 DFG = TrowbridgeReitzNDF(nDotH, alpha) * SchlickBeckmanGSF(nDotL, nDotV, alpha) * F;

    vec3 specular = DFG / denom;

    vec3 color = nDotL * (diffuse + specular);

    color = pow(color, vec3(1.0 / 2.2));

    oColor = vec4(color, 1.0);
}

#endif
