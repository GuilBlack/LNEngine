#pragma once

namespace lne
{

enum class EShaderStage : byte
{
    Vertex = 0,
    TessellationControl = 1,
    TessellationEvaluation = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,
};

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
