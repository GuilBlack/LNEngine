#pragma once
#include "Enums.h"

namespace lne
{
struct UniformBuffer
{
    uint32_t SetIndex;
    uint32_t BindingIndex;
    uint32_t Size;
    ShaderStage::Enum Stages;
};

struct UniformElement
{
    std::string UniformBuffer;
    uint32_t Offset;
    uint32_t Size;
    UniformElementType::Enum Type;
};

}
