#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"

namespace lne
{
struct WorldData
{
    glm::mat4           ViewProj;
    glm::mat4           View;
    glm::mat4           Proj;
    glm::vec3           CameraPosition;
    float               AmbientLight;
    BindlessImageHandle BRDFLut;
    BindlessImageHandle IrradianceMap;
    BindlessImageHandle PrefilteredMap;
    bool                IsSunEnabled;
};

// TODO: move this in the World renderer
struct FrameData
{
    SafePtr<class UniformBuffer>                CurrentWorldDataUniforms;
    SafePtr<class DynamicDescriptorAllocator>   DescriptorAllocator;
    vk::DescriptorSet                           DescriptorSet{};
    vk::DescriptorSetLayout                     DescriptorSetLayout;
    WorldData                                   CurrentWorldData{};

    FrameData(SafePtr<class DynamicDescriptorAllocator> descriptorAllocator,
              vk::DescriptorSetLayout descriptorSetLayout);
};
}
