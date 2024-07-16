#pragma once

#pragma once
#include <VkBootstrap.h>
#include "PhysicalDevice.h"
#include "Device.h"

namespace lne
{
class Device
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> GraphicsFamily;
        std::optional<uint32_t> ComputeFamily;
        std::optional<uint32_t> TransferFamily;
        std::optional<uint32_t> PresentFamily;

        [[nodiscard]] bool IsComplete() const
        {
            return GraphicsFamily.has_value() 
                && ComputeFamily.has_value()
                && TransferFamily.has_value()
                && PresentFamily.has_value();
        }
    };
public:
    Device(vkb::PhysicalDevice& physicalDevice, class vk::SurfaceKHR surface);
    ~Device();

    [[nodiscard]] const class vk::Device& GetHandle() const { return m_Device; }
    [[nodiscard]] const vk::Queue& GetGraphicsQueue() const { return m_GraphicsQueue; }
    [[nodiscard]] const vk::Queue& GetComputeQueue() const { return m_ComputeQueue; }
    [[nodiscard]] const vk::Queue& GetTransferQueue() const { return m_TransferQueue; }
    [[nodiscard]] const vk::Queue& GetPresentQueue() const { return m_PresentQueue; }

    [[nodiscard]] const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
private:
    class vk::Device m_Device;
    QueueFamilyIndices m_QueueFamilyIndices;

    vk::Queue m_GraphicsQueue;
    vk::Queue m_ComputeQueue;
    vk::Queue m_TransferQueue;
    vk::Queue m_PresentQueue;
};
}
