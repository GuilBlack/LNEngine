#pragma once

namespace lne
{
namespace vkut
{

#define VK_CHECK(func)                                                                 \
  {                                                                                    \
    const vk::Result result = func;                                                    \
    if (result != vk::Result::eSuccess) {                                              \
      std::cerr << "Error calling function " << #func << " at " << __FILE__ << ":"     \
                << __LINE__ << ". Result is " << result << std::endl; \
      assert(false);                                                                   \
    }                                                                                  \
  }

std::vector<std::string> filterExtensions(std::vector<std::string>& requiredExtensions, std::vector<std::string>& availableExtensions);

vk::PresentModeKHR chooseSwapPresentMode();

}
}
