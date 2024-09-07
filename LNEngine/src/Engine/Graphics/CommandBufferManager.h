#pragma once
#include "GfxEnums.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{
class CommandBufferManager
{
public:
    CommandBufferManager(class GfxContext* ctx, uint32_t count, EQueueFamilyType queueType);
    ~CommandBufferManager();

    [[nodiscard]] vk::CommandBuffer& GetCurrentCommandBuffer() 
    {
        return m_CommandBuffers[m_CurrentBufferIndex]; 
    }
    [[nodiscard]] bool GetFenceStatus(uint32_t index);
    void StartCommandBuffer(uint32_t index);

    void Submit(vk::SubmitInfo& submitInfo, uint32_t index = UINT32_MAX);

    vk::CommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands();

private:
    GfxContext* m_Context;
    vk::Queue m_Queue;
    vk::CommandPool m_CommandPool;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    std::vector<vk::Fence> m_WaitFences;

    uint32_t m_CurrentBufferIndex{ 0 };

private:
    [[nodiscard]] std::vector<vk::CommandBuffer> AllocateCommandBuffers(uint32_t count, std::string_view cbName);
    [[nodiscard]] vk::CommandBuffer AllocateCommandBuffer(std::string_view cbName);
};
}
