#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/UniformBuffer.h"
#include "Engine/Graphics/Structs.h"

namespace lne
{
struct WorldData
{
    glm::mat4           ViewProj;
    glm::mat4           View;
    glm::mat4           Proj;
    glm::vec3           CameraPosition;
    glm::vec3           SunDirection;
    float               AmbientLight;
    BindlessImageHandle BRDFLut;
    BindlessImageHandle IrradianceMap;
    BindlessImageHandle PrefilteredMap;
};

// TODO: move this in the World renderer
struct FrameData
{
    SafePtr<UniformBuffer>                      CurrentWorldDataUniforms;
    SafePtr<class DynamicDescriptorAllocator>   DescriptorAllocator;
    vk::DescriptorSet                           DescriptorSet;
    vk::DescriptorSetLayout                     DescriptorSetLayout;
    WorldData                                   CurrentWorldData;

    FrameData(SafePtr<class DynamicDescriptorAllocator> descriptorAllocator,
        vk::DescriptorSetLayout descriptorSetLayout)
        : CurrentWorldDataUniforms(nullptr),
        DescriptorAllocator(descriptorAllocator),
        DescriptorSetLayout(descriptorSetLayout)
    {}
};
}
