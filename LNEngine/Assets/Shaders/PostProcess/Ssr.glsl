//#lne_head [Vt main][Fg main][Rp LightingPass][Tp PostProcess]
#version 460

#include "Common.glslh"
#include "CommonPostProcess.glslh"

struct Material {
    uint tScene;
    uint tAlbedo;
    uint tMetalnessRoughness;
    uint tNormal;
    uint tDepth;
};

layout(scalar, push_constant) uniform MatPC {
    uint id;
} matPC;

layout(scalar, set = MAT_SET, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

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

vec3 samplePrefilteredReflection(in vec3 reflectDir, float roughness) {
    float maxReflLod = log2(float(textureSize(globalTextures[nonuniformEXT(tPrefilteredMap)], 0).x));
    float lod = maxReflLod * roughness;
    float lodMin = floor(lod);
    float lodMax = ceil(lod);
    vec3 sample1 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMin).xyz;
    vec3 sample2 = textureLod(globalCubemaps[nonuniformEXT(tPrefilteredMap)], reflectDir, lodMax).xyz;
    return mix(sample1, sample2, lod - lodMin);
}

vec3 sampleSceneReflection(in vec2 uv, float roughness, uint sceneHandle)
{
    vec2 ts = textureSize(globalTextures[nonuniformEXT(sceneHandle)], 0);
    float maxLod = floor(log2(max(ts.x, ts.y)));

    float pr = clamp(roughness, 0.0, 1.0);
    float lod = clamp(maxLod * (pr * pr), 0.0, maxLod);

    float lodMin = floor(lod);
    float lodMax = min(lodMin + 1.0, maxLod);

    vec3 a = textureLod(globalTextures[nonuniformEXT(sceneHandle)], uv, lodMin).rgb;
    vec3 b = textureLod(globalTextures[nonuniformEXT(sceneHandle)], uv, lodMax).rgb;

    return mix(a, b, lod - lodMin);
}

// https://www.shadertoy.com/view/MslGR8 for dithering (maybe)
#define MOD3 vec3(443.8975,397.2973, 491.1871)
float hash12(vec2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    float maxDist           = 20; // max ray distance
    float res               = 0.3; // % of the pixels to use
    int maxCoarseMarchSteps = 128; // for the coarse pass
    int numSteps            = 10; // for the binary search pass
    float thickness         = 0.5; // in world units
    thickness               *= -1.0; // because view space Z is negative
    Material mat = materials[matPC.id];

    vec4 nWorldAndMask = texture(globalTextures[nonuniformEXT(mat.tNormal)], iUV);
    vec3 worldNormal = normalize(nWorldAndMask.xyz);
    float mask = nWorldAndMask.w;
    vec3 sceneColor = textureLod(globalTextures[nonuniformEXT(mat.tScene)], iUV, 0).xyz;

    vec3 metalnessRoughness = texture(globalTextures[nonuniformEXT(mat.tMetalnessRoughness)], iUV).xyz;
    float metalness = metalnessRoughness.x;
    float roughness = metalnessRoughness.y;
    vec3 albedo = texture(globalTextures[nonuniformEXT(mat.tAlbedo)], iUV).xyz;

    float depth = texture(globalTextures[nonuniformEXT(mat.tDepth)], iUV).r;
    if (depth == 0.0 || mask < 0.99999999) {
        oColor = vec4(sceneColor, 1.0);
        return;
    }

    vec3 normal = (uView * vec4(worldNormal, 0.0)).xyz;
    vec2 screenRes = textureSize(globalTextures[nonuniformEXT(mat.tScene)], 0).xy;
    
    mat4 inverseProj = inverse(uProj);
    vec3 camToPos = getViewPositionFromDepth(depth, iUV, inverseProj);
    vec3 worldPos = getPositionFromDepth(depth, iUV, uViewProj);
    vec3 ndcPos = getNDCPositionFromDepth(depth, iUV);
    vec3 dir = normalize(camToPos);
    vec3 rayDir = normalize(reflect(dir, normalize(normal)));

    vec4 rayStart = vec4(camToPos, 1.0);
    vec4 rayEnd = vec4(camToPos + rayDir * maxDist, 1.0);
    
    vec4 startFrag = vec4(ndcPos, 1.0);
    startFrag.xy = startFrag.xy * 0.5 + 0.5;
    startFrag.y = 1.0 - startFrag.y; // flip Y for texture coords
    startFrag.xy *= screenRes;

    vec4 endFrag = vec4(camToPos, 1.0) + vec4(rayDir * maxDist, 0.0);
    endFrag = uProj * endFrag;
    endFrag /= endFrag.w;
    endFrag.xy = endFrag.xy * 0.5 + 0.5;
    endFrag.y = 1.0 - endFrag.y; // flip Y for texture coords
    endFrag.xy *= screenRes;

    vec2 rayLengthPx = endFrag.xy - startFrag.xy;
    float useX = abs(rayLengthPx.x) >= abs(rayLengthPx.y) ? 1.0 : 0.0;
    float delta = mix(abs(rayLengthPx.y), abs(rayLengthPx.x), useX) * clamp(res, 0, 1);
    delta = min(delta, float(maxCoarseMarchSteps));
    vec2 rayStep = rayLengthPx / max(delta, 0.001);

    float lastMiss = 0.0;
    float lerpVal = 0.0;

    int firstPassIntersection = 0;
    int secondPassIntersection = 0;

    float viewDist = -rayStart.z; // the distance from the camera from the current point on the ray
    depth = thickness; // depth from the ray to the sampled pixel position

    vec2 currFrag = startFrag.xy;
    vec2 currUv = vec2(0.0);
    vec3 marchPos = vec3(0.0);

    for (int i = 0; i < int(delta); ++i) {
        currFrag += rayStep;
        currUv = currFrag / screenRes;
        marchPos = getViewPositionFromDepth(texture(globalTextures[nonuniformEXT(mat.tDepth)], currUv).r, currUv, inverseProj);
        lerpVal = useX == 1 ? (currFrag.x - startFrag.x) / rayLengthPx.x : (currFrag.y - startFrag.y) / rayLengthPx.y;
        lerpVal = clamp(lerpVal, 0.0, 1.0);
        
        viewDist = (rayStart.z * rayEnd.z) / mix(rayEnd.z, rayStart.z, lerpVal);
        depth =  viewDist - marchPos.z;

        if (depth < 0.0 && depth > thickness) {
            firstPassIntersection = 1;
            break;
        }
        else {
            lastMiss = lerpVal;
        }
    }

    float hitDepth = 0.0;

    lerpVal = lastMiss + (lerpVal - lastMiss) / 2.0;
    numSteps *= firstPassIntersection;

    for (int i = 0; i < numSteps; ++i) {
        currFrag = mix(startFrag.xy, endFrag.xy, lerpVal);
        currUv = currFrag / screenRes;
        float realDepth = texture(globalTextures[nonuniformEXT(mat.tDepth)], currUv).r;
        marchPos = getViewPositionFromDepth(realDepth, currUv, inverseProj);

        viewDist = (rayStart.z * rayEnd.z) / mix(rayEnd.z, rayStart.z, lerpVal);
        depth =  viewDist - marchPos.z;

        // binary search
        if (depth < 0.0 && depth > thickness) {
            secondPassIntersection = 1;
            lerpVal = lastMiss + (lerpVal - lastMiss) / 2.0;
            hitDepth = realDepth;
        }
        else {
            float prevLerp = lerpVal;
            lerpVal = lerpVal + (lerpVal - lastMiss) / 2.0;
            lastMiss = prevLerp;
        }
    }
    float distWeight = length(marchPos - camToPos) / maxDist; // if it's far away, the visibility is reduced
    distWeight = (1.0 - clamp(distWeight*distWeight*distWeight, 0.0, 1.0));
    float edgeDist = min(min(currUv.x, 1.0 - currUv.x), min(currUv.y, 1.0 - currUv.y));
    float edgeWeight = smoothstep(0.0, 0.02, edgeDist); // if the ray hits outside the screen

    float facing = dot(-dir, rayDir);
    float facingWeight = 1.0 - smoothstep(0.1, 0.4, facing);  // if the ray is facing the camera, reduce visibility

    float t = clamp(depth / thickness, 0.0, 1.0);
    float thicknessWeight = 1.0 - smoothstep(0.0, 1.0, t); // the deeper the intersection, the less visible

    float weight = secondPassIntersection * (hitDepth > 0.0 ? 1.0 : 0.0) // my depth is 0.0 for far plane, so if I hit it, no visibility
         * facingWeight * thicknessWeight * distWeight * edgeWeight;

    weight = clamp(weight, 0.0, 1.0);

    vec3 viewDir = normalize(uEyePos - worldPos);
    vec3 reflectDir = reflect(-viewDir, worldNormal);
    float nDotV = max(0.0, dot(worldNormal, viewDir));

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metalness);
    vec3 F = FresnelSchlick(nDotV, F0);
    
    vec2 brdf = texture(globalTextures[nonuniformEXT(tBRDFLut)], vec2(nDotV, roughness)).xy;

    vec3 prefilteredColor = samplePrefilteredReflection(reflectDir, roughness);
    vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

    //vec3 reflectedColor = textureLod(globalTextures[nonuniformEXT(mat.tScene)], currUv, 0).xyz;
    vec3 reflectedColor = sampleSceneReflection(currUv, roughness, mat.tScene);

    reflectedColor = reflectedColor * (F * brdf.x + brdf.y);

    float weightR = 1.0 - roughness * roughness; // to kill ssr on rough surfaces since I don't have a convolved mip chain for my scene
    weight *= clamp(weightR, 0.0, 1.0);

    float weightF = clamp(max(F.r, max(F.g, F.b)), 0.0, 1.0);
    weight *= clamp(weightF, 0.0, 1.0);

    oColor = vec4(sceneColor + weight * (reflectedColor - specularIBL), weight);
}

#endif
