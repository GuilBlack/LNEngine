//#lne_head [Vt main][Fg main][Rp LightingPass][Tp PostProcess]
#version 460

#include "Common.glslh"
#include "CommonPostProcess.glslh"

struct Material {
    uint tScene;
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

void main() {
    float maxDist           = 20; // max ray distance
    float res               = 0.3; // % of the pixels to use
    int maxCoarseMarchSteps = 128; // for the coarse pass
    int numSteps            = 10; // for the binary search pass
    float thickness         = 0.5; // in world units
    thickness               *= -1.0; // because view space Z is negative
    Material mat = materials[matPC.id];

    vec4 nWorldAndMask = texture(globalTextures[nonuniformEXT(mat.tNormal)], iUV);
    float mask = nWorldAndMask.w;
    vec3 sceneColor = texture(globalTextures[nonuniformEXT(mat.tScene)], iUV).xyz;

    float depth = texture(globalTextures[nonuniformEXT(mat.tDepth)], iUV).r;
    if (depth == 0.0 || mask < 0.75) {
        oColor = vec4(sceneColor, 1.0);
        return;
    }

    vec3 normal = (uView * vec4(normalize(nWorldAndMask.xyz), 0.0)).xyz;
    vec2 screenRes = textureSize(globalTextures[nonuniformEXT(mat.tScene)], 0).xy;
    
    mat4 inverseProj = inverse(uProj);
    vec3 camToPos = getViewPositionFromDepth(depth, iUV, inverseProj);
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
    vec2 step = rayLengthPx / max(delta, 0.001);

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
        currFrag += step;
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

    float visibility = secondPassIntersection * (hitDepth > 0.0 ? 1.0 : 0.0) // my depth is 0.0 for far plane, so if I hit it, no visibility
         * (1.0 - max(dot(-dir, rayDir), 0.0)) // if the ray is facing the camera, reduce visibility
         * (1.0 - clamp(depth / thickness, 0.0, 1.0)) // the deeper the intersection, the less visible
         * (1.0 - clamp(length(marchPos - camToPos) / maxDist, 0.0, 1.0)) // if it's far away, the visibility is reduced
         * (currUv.x < 0.0 || currUv.x > 1.0 ? 0.0 : 1.0)  // outside of screen
         * (currUv.y < 0.0 || currUv.y > 1.0 ? 0.0 : 1.0); // outside of screen
    
    //oColor = vec4(currUv, visibility, visibility);
    vec3 reflectedColor = texture(globalTextures[nonuniformEXT(mat.tScene)], currUv).xyz;
    oColor = vec4(mix(sceneColor, reflectedColor, visibility < 0.5 ? 0.0 : 1.0), 1.0);
}

#endif
