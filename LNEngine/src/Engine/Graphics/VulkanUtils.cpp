#include "lnepch.h"
#include "VulkanUtils.h"

namespace lne
{
namespace vkut
{
std::vector<std::string> FilterExtensions(std::vector<std::string>& requiredExtensions, std::vector<std::string>& availableExtensions)
{
    std::vector<std::string> result;
    std::sort(requiredExtensions.begin(), requiredExtensions.end());
    std::sort(availableExtensions.begin(), availableExtensions.end());
    std::set_intersection(requiredExtensions.begin(), requiredExtensions.end(), availableExtensions.begin(), availableExtensions.end(), std::back_inserter(result));
    return result;
}

vk::SemaphoreSubmitInfo CreateSemaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore)
{
    vk::SemaphoreSubmitInfo info{};
    info.semaphore = semaphore;
    info.stageMask = stageMask;
    info.deviceIndex = 0;
    info.value = 1;

    return info;
}
vk::SubmitInfo2 SubmitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo, vk::SemaphoreSubmitInfo* waitSemaphoreInfo)
{
    vk::SubmitInfo2 info = {};
    info.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1;
    info.pWaitSemaphoreInfos = waitSemaphoreInfo;

    info.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1;
    info.pSignalSemaphoreInfos = signalSemaphoreInfo;

    info.commandBufferInfoCount = 1;
    info.pCommandBufferInfos = cmd;
    return info;
}
vk::CommandBufferSubmitInfo CommandBufferSubmitInfo(vk::CommandBuffer cmdBuffer)
{
    return vk::CommandBufferSubmitInfo(cmdBuffer);
}
bool IsDepthFormat(vk::Format format)
{
    return (format == vk::Format::eD16Unorm
        || format == vk::Format::eD32Sfloat
        || format == vk::Format::eD16UnormS8Uint
        || format == vk::Format::eD24UnormS8Uint
        || format == vk::Format::eD32SfloatS8Uint);
}
bool IsStencilFormat(vk::Format format)
{
    return (format == vk::Format::eS8Uint
        || format == vk::Format::eD16UnormS8Uint
        || format == vk::Format::eD24UnormS8Uint
        || format == vk::Format::eD32SfloatS8Uint);
}

vk::ImageUsageFlags GetImageUsageFlags(TextureUsageType::Enum usageType)
{
    switch (usageType)
    {
    case TextureUsageType::eSampled:
        return vk::ImageUsageFlagBits::eSampled;
    case TextureUsageType::eStorage:
        return vk::ImageUsageFlagBits::eStorage;
    case TextureUsageType::eSampledAndStorage:
        return vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    default:
        return {};
    }
}
}
}
