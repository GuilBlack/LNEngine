#pragma once

namespace lne
{
namespace ShaderStage
{
enum Enum : byte
{
    eVertex = 0,
    eTessellationControl = 1,
    eTessellationEvaluation = 2,
    eGeometry = 3,
    eFragment = 4,
    eCompute = 5,
};

enum Mask
{
    mVertex = 1 << 0,
    mTessellationControl = 1 << 1,
    mTessellationEvaluation = 1 << 2,
    mGeometry = 1 << 3,
    mFragment = 1 << 4,
    mCompute = 1 << 5,
};

    extern const char** s_Enum;

inline std::string_view ToString(Enum type)
{
    return s_Enum[type];
}

inline Mask ToMask(Enum type)
{
    return (Mask)(1 << type);
}
}

namespace UniformElementType
{
    enum Enum : byte
    {
        eUnknown,
        eFloat,eFloat2,eFloat3,eFloat4,
        eInt,eInt2,eInt3,eInt4,
        eUInt,eUInt2,eUInt3,eUInt4,
        eMatrix2x2,eMatrix3x3,eMatrix4x4
    };

    extern const char** s_Enum;

    inline std::string_view ToString(Enum type)
    {
        return s_Enum[type];
    }
}

enum class EFillMode : byte
{
    Solid = 0,
    Wireframe = 1,
    Point = 2
};

enum class EBlendColorWriteMask : byte
{
    Red = 1 << 0,
    Green = 1 << 1,
    Blue = 1 << 2,
    Alpha = 1 << 3,
    All = Red | Green | Blue | Alpha
};

enum class ECullMode : byte
{
    None = 0,
    Front = 1,
    Back = 2,
    FrontAndBack = 3
};

enum class EWindingOrder : byte
{
    CounterClockwise = 0,
    Clockwise = 1,
};

enum class ECompareOperation : byte
{
    Never = 0,
    Less = 1,
    Equal = 2,
    LessOrEqual = 3,
    Greater = 4,
    NotEqual = 5,
    GreaterOrEqual = 6,
    Always = 7
};
}
