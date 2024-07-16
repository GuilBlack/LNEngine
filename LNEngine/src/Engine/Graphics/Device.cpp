#include "Device.h"
#include "Core/Utils/_Defines.h"
#include "Core/Utils/Log.h"

namespace lne
{
lne::Device::Device(vkb::PhysicalDevice& physicalDevice, vk::SurfaceKHR surface)
{
    auto deviceRet = vkb::DeviceBuilder{ physicalDevice }.build();
    LNE_ASSERT(deviceRet, "Failed to create device");
    auto deviceVal = deviceRet.value();
    m_Device = deviceRet.value().device;
    auto computeFamilyRet = deviceVal.get_dedicated_queue_index(vkb::QueueType::compute);
    if (!computeFamilyRet)
    {
        computeFamilyRet = deviceVal.get_queue_index(vkb::QueueType::compute);
        m_QueueFamilyIndices.ComputeFamily = computeFamilyRet.value();
        m_ComputeQueue = deviceVal.get_queue(vkb::QueueType::compute).value();
    }
    else
    {
        m_QueueFamilyIndices.ComputeFamily = computeFamilyRet.value();
        m_ComputeQueue = deviceVal.get_dedicated_queue(vkb::QueueType::compute).value();
    }

    auto transferFamilyRet = deviceVal.get_dedicated_queue_index(vkb::QueueType::transfer);
    if (!transferFamilyRet)
    {
        transferFamilyRet = deviceVal.get_queue_index(vkb::QueueType::transfer);
        m_QueueFamilyIndices.TransferFamily = transferFamilyRet.value();
        m_TransferQueue = deviceVal.get_queue(vkb::QueueType::transfer).value();
    }
    else
    {
        m_QueueFamilyIndices.TransferFamily = transferFamilyRet.value();
        m_TransferQueue = deviceVal.get_dedicated_queue(vkb::QueueType::transfer).value();
    }

    m_QueueFamilyIndices.GraphicsFamily = deviceVal.get_queue_index(vkb::QueueType::graphics).value();
    m_GraphicsQueue = deviceVal.get_queue(vkb::QueueType::graphics).value();
    m_QueueFamilyIndices.PresentFamily = deviceVal.get_queue_index(vkb::QueueType::present).value();
    m_PresentQueue = deviceVal.get_queue(vkb::QueueType::present).value();
    
    LNE_ASSERT(m_QueueFamilyIndices.IsComplete(), "Failed to find all queue families");

    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);
}

Device::~Device()
{
    m_Device.destroy();
}
}
