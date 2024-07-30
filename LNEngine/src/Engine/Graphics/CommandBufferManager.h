#pragma once
#include "GfxEnums.h"

namespace lne
{
class CommandBufferManager
{
public:
    CommandBufferManager(std::shared_ptr<class GfxContext> ctx, uint32_t count, EQueueFamilyType queueType);
    ~CommandBufferManager();

    [[nodiscard]] vk::CommandBuffer& GetCurrentCommandBuffer() { return m_CommandBuffers[m_CurrentImageIndex]; }
    void StartCommandBuffer(uint32_t index);

    void Submit(const vk::SubmitInfo& submitInfo);

private:
    std::shared_ptr<class GfxContext> m_Context;
    vk::Queue m_Queue;
    vk::CommandPool m_CommandPool;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    std::vector<vk::Fence> m_WaitFences;

    uint32_t m_CurrentImageIndex{ 0 };

private:
    [[nodiscard]] std::vector<vk::CommandBuffer> AllocateCommandBuffers(uint32_t count, std::string_view cbName);
    [[nodiscard]] vk::CommandBuffer AllocateCommandBuffer(std::string_view cbName);
};
}
