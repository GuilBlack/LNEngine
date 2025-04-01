#version 460

#extension GL_EXT_nonuniform_qualifier : require

#ifdef  VERT

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(push_constant) uniform uPushConstant {
    vec2 uScale;
    vec2 uTranslate;
    uint uTextureIndex;
} pc;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out struct {
    vec4 Color;
    vec2 UV;
} Out;

layout(location = 2) flat out uint TextureIndex;

void main()
{
    Out.Color = aColor;
    Out.UV = aUV;
    TextureIndex = pc.uTextureIndex;
    gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
}

#endif

#ifdef FRAG

layout(location = 0) out vec4 fColor;

layout(set=0, binding=0) uniform sampler2D globalTextures[];

layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

layout(location = 2) flat in uint TextureIndex;

void main()
{
    fColor = In.Color * texture(globalTextures[nonuniformEXT(TextureIndex)], In.UV.st);
}

#endif