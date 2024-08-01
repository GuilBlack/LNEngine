#pragma once

namespace lne
{
namespace vkut
{

#define VK_CHECK(func)                                                                 \
  {                                                                                    \
    vk::Result checkResult = func;                                                    \
    if (checkResult != vk::Result::eSuccess) {                                              \
      std::cerr << "Error calling function " << #func << " at " << __FILE__ << ":"     \
                << __LINE__ << ". Result is " << checkResult << std::endl; \
      assert(false);                                                                   \
    }                                                                                  \
  }

std::vector<std::string> FilterExtensions(std::vector<std::string>& requiredExtensions, std::vector<std::string>& availableExtensions);

vk::SemaphoreSubmitInfo CreateSemaphoreSubmitInfo(vk::PipelineStageFlags2 stageMask, vk::Semaphore semaphore);

vk::SubmitInfo2 SubmitInfo(vk::CommandBufferSubmitInfo* cmd, vk::SemaphoreSubmitInfo* signalSemaphoreInfo,
    vk::SemaphoreSubmitInfo* waitSemaphoreInfo);

vk::CommandBufferSubmitInfo CommandBufferSubmitInfo(vk::CommandBuffer cmdBuffer);

}
}
