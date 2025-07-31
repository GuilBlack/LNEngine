#include "Enums.h"

namespace lne
{
namespace ShaderStage
{
const char* enumValues[7] = {
    "Vertex",
    "TessellationControl",
    "TessellationEvaluation",
    "Geometry",
    "Fragment",
    "Compute",
    "Unknown"
};
const char** s_Enum = enumValues;
}

namespace UniformElementType
{
const char* enumValues[17] = {
        "Unknown",
        "Float","Float2","Float3","Float4",
        "Int","Int2","Int3","Int4",
        "UInt","UInt2","UInt3","UInt4",
        "Matrix2x2","Matrix3x3","Matrix4x4"
};
const char** s_Enum = enumValues;
}

namespace FrameGraphResourceType
{
const char* enumValues[5] = {
    "Unknown",
    "Texture",
    "Buffer",
    "Attachment",
    "Proxy"
};
const char** s_Enum = enumValues;
}

namespace ResourceType
{
const char* enumValues[6] = {
    "Unknown",
    "Texture",
    "Buffer",
    "Pipeline",
    "Shader",
    "ImageView"
};
const char** s_Enum = enumValues;
}
}
