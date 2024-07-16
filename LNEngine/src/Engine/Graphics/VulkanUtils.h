#pragma once

namespace lne
{
namespace vkut
{

std::vector<std::string> filterExtensions(std::vector<std::string>& requiredExtensions, std::vector<std::string>& availableExtensions)
{
    std::vector<std::string> result;
    std::sort(requiredExtensions.begin(), requiredExtensions.end());
    std::sort(availableExtensions.begin(), availableExtensions.end());
    std::set_intersection(requiredExtensions.begin(), requiredExtensions.end(), availableExtensions.begin(), availableExtensions.end(), std::back_inserter(result));
    return result;
}

}
}
