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

void main() {
  vec3 albedo = texture(globalTextures[tAlbedo], iUVs).xyz;
  vec3 lighting = vec3(0.0);
  vec3 normal = normalize(iNormal);
  vec3 viewDir = normalize(uEyePos - iWorldPos);

  // Ambient
  vec3 ambient = vec3(0.01);

  // Diffuse lighting
  vec3 lightDir = normalize(uSunDir);
  vec3 lightColor = vec3(1.0);
  float cosTheta = max(0.0, dot(lightDir, normal));

  vec3 diffuse = cosTheta * lightColor;

  // Phong specular
  vec3 r = normalize(reflect(-lightDir, normal));
  float spec = max(0.0, dot(viewDir, r));
  spec = pow(spec, 32.0);

  lighting = ambient + diffuse * 0.8;

  vec3 color = albedo * lighting + vec3(spec);

  color = pow(color, vec3(1.0 / 2.2));

  oColor = vec4(color, 1.0);
}

#endif
