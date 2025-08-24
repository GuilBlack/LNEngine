
#include "GlobalGfxData.h"
#include "DynamicDescriptorAllocator.h"
#include "Graphics/Resources/UniformBuffer.h"

namespace lne
{
FrameData::FrameData(SafePtr<class DynamicDescriptorAllocator> descriptorAllocator, vk::DescriptorSetLayout descriptorSetLayout)
    : CurrentWorldDataUniforms(nullptr),
    DescriptorAllocator(descriptorAllocator),
    DescriptorSetLayout(descriptorSetLayout)
{}
}

