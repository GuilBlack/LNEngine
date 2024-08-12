#pragma once
#include "Enums.h"

namespace lne
{
struct UniformBuffer
{
    uint32_t SetIndex;
    uint32_t BindingIndex;
    uint32_t Size;
    vk::ShaderStageFlags Stages;
};

struct DescriptorSet
{
    uint32_t SetIndex;
    std::unordered_map<std::string, UniformBuffer> UniformBuffers;
};

struct UniformElement
{
    std::string UniformBuffer;
    uint32_t Offset;
    uint32_t Size;
    UniformElementType::Enum Type;
};
}
