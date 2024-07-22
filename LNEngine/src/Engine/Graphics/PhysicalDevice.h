#pragma once
#include <VkBootstrap.h>

namespace lne
{
class PhysicalDevice
{
public:
    static vkb::PhysicalDevice VkbSelectPhysicalDevice(const vkb::Instance& instance, vk::SurfaceKHR surface);

    PhysicalDevice(vkb::PhysicalDevice& physicalDevice);
    ~PhysicalDevice() = default;

    [[nodiscard]] const class vk::PhysicalDevice& GetHandle() const { return m_PhysicalDevice; }
    [[nodiscard]] const vk::PhysicalDeviceProperties& GetProperties() const { return m_Properties; }
    [[nodiscard]] const vk::PhysicalDeviceFeatures& GetEnabledFeatures() const { return m_EnabledFeatures; }

    [[nodiscard]] const vk::PhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemoryProperties; }

    [[nodiscard]] const std::vector<vk::QueueFamilyProperties>& GetQueueFamilyProperties() const { return m_QueueFamilyProperties; }

    [[nodiscard]] vk::SurfaceCapabilitiesKHR GetSurfaceCapabilities(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::SurfaceFormatKHR> GetSurfaceFormats(vk::SurfaceKHR surface) const;
    [[nodiscard]] std::vector<vk::PresentModeKHR> GetSurfacePresentModes(vk::SurfaceKHR surface) const;
    

private:
    class vk::PhysicalDevice m_PhysicalDevice;
    vk::PhysicalDeviceProperties m_Properties;
    vk::PhysicalDeviceFeatures m_EnabledFeatures;
    vk::PhysicalDeviceMemoryProperties m_MemoryProperties;
    std::vector<vk::QueueFamilyProperties> m_QueueFamilyProperties;
};
}
