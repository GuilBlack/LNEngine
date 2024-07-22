#include "lnepch.h"
#include "VulkanUtils.h"

std::vector<std::string> lne::vkut::filterExtensions(std::vector<std::string>& requiredExtensions, std::vector<std::string>& availableExtensions)
{
    std::vector<std::string> result;
    std::sort(requiredExtensions.begin(), requiredExtensions.end());
    std::sort(availableExtensions.begin(), availableExtensions.end());
    std::set_intersection(requiredExtensions.begin(), requiredExtensions.end(), availableExtensions.begin(), availableExtensions.end(), std::back_inserter(result));
    return result;
}

vk::PresentModeKHR lne::vkut::chooseSwapPresentMode()
{
    return vk::PresentModeKHR();
}
