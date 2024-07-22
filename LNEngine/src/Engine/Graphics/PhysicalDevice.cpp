#include "PhysicalDevice.h"
#include "GfxContext.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Log.h"

namespace lne
{
vkb::PhysicalDevice PhysicalDevice::VkbSelectPhysicalDevice(const vkb::Instance& instance, vk::SurfaceKHR surface)
{
    auto physDeviceSelect = vkb::PhysicalDeviceSelector(instance);

    auto deviceFeatures = VkPhysicalDeviceFeatures{
        .imageCubeArray = vk::True,
        .geometryShader = vk::True, // for im3d
        .depthClamp = vk::True,
        .samplerAnisotropy = vk::True,
    };
    
    auto features12 = VkPhysicalDeviceVulkan12Features{
        .descriptorIndexing = vk::True,
    };

    auto features13 = VkPhysicalDeviceVulkan13Features{
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    physDeviceSelect.set_surface(surface)
        .set_minimum_version(1, 3)
        .set_required_features(deviceFeatures)
        .set_required_features_12(features12)
        .set_required_features_13(features13);

    vkb::Result<vkb::PhysicalDevice> selectedDevice = physDeviceSelect.select();

    LNE_ASSERT(selectedDevice, "Failed to select a physical device");

    return selectedDevice.value();
}

PhysicalDevice::PhysicalDevice(vkb::PhysicalDevice& physicalDevice)
{
    m_PhysicalDevice = physicalDevice.physical_device;
    m_EnabledFeatures = physicalDevice.features;
    m_Properties = m_PhysicalDevice.getProperties();
    m_MemoryProperties = physicalDevice.memory_properties;
    m_QueueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();
}

vk::SurfaceCapabilitiesKHR PhysicalDevice::GetSurfaceCapabilities(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfaceCapabilitiesKHR(surface);
}

std::vector<vk::SurfaceFormatKHR> PhysicalDevice::GetSurfaceFormats(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfaceFormatsKHR(surface);
}
std::vector<vk::PresentModeKHR> PhysicalDevice::GetSurfacePresentModes(vk::SurfaceKHR surface) const
{
    return m_PhysicalDevice.getSurfacePresentModesKHR(surface);
}
}
