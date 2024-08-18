//#lne_head [[Vt main][Fg main]]
#version 460

layout(std140, set=0, binding=0) uniform GlobalUBO {
    mat4 uViewProj;
    mat4 uView;
    mat4 uProj;
};

#ifdef VERT

layout(location = 0) out vec4 oColor;

vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5));

vec3 colors[3] = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

void main() {
  gl_Position = uViewProj * vec4(positions[gl_VertexIndex], 0.0, 1.0);
  oColor = vec4(colors[gl_VertexIndex], 1.0);
}

#endif

#ifdef FRAG

layout(location = 0) in vec4 iColor;
layout(location = 0) out vec4 oColor;

void main() {
  oColor = iColor;
}

#endif
