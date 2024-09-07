#include "CommandBufferManager.h"
#include "GfxContext.h"
#include "Core/SafePtr.h"

namespace lne
{
CommandBufferManager::CommandBufferManager(GfxContext* ctx, uint32_t count, EQueueFamilyType queueType)
    : m_Context(ctx), m_Queue(ctx->GetQueue(queueType))
{
    auto device = m_Context->GetDevice();
    m_CommandPool = m_Context->CreateCommandPool(ctx->GetQueueFamilyIndex(queueType));
    m_CommandBuffers = AllocateCommandBuffers(count, ctx->GetQueueFamilyName(queueType));

    m_WaitFences.resize(count);
    vk::FenceCreateInfo fenceCI{ vk::FenceCreateFlagBits::eSignaled };
    for (auto& fence : m_WaitFences)
    {
        fence = device.createFence(fenceCI);
        m_Context->SetVkObjectName(fence, "Swapchain Fence");
    }
}

CommandBufferManager::~CommandBufferManager()
{
    auto device = m_Context->GetDevice();

    for (auto& fence : m_WaitFences)
        device.destroyFence(fence);

    device.freeCommandBuffers(m_CommandPool, m_CommandBuffers);
    device.destroyCommandPool(m_CommandPool);
}

bool CommandBufferManager::GetFenceStatus(uint32_t index)
{
    return m_Context->GetDevice().getFenceStatus(m_WaitFences[index]) == vk::Result::eSuccess;
}

void CommandBufferManager::StartCommandBuffer(uint32_t index)
{
    m_CurrentBufferIndex = index;
    auto device = m_Context->GetDevice();
    VK_CHECK(device.waitForFences(m_WaitFences[m_CurrentBufferIndex], VK_TRUE, UINT64_MAX));

    m_CommandBuffers[m_CurrentBufferIndex].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
    m_CommandBuffers[m_CurrentBufferIndex].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
}

void CommandBufferManager::Submit(vk::SubmitInfo& submitInfo, uint32_t index)
{
    if (index == UINT32_MAX)
        index = m_CurrentBufferIndex;
    m_Context->GetDevice().resetFences(m_WaitFences[m_CurrentBufferIndex]);
    m_CommandBuffers[m_CurrentBufferIndex].end();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentBufferIndex];
    m_Queue.submit(submitInfo, m_WaitFences[m_CurrentBufferIndex]);
}

vk::CommandBuffer CommandBufferManager::BeginSingleTimeCommands()
{
    uint32_t index = (uint32_t)m_CommandBuffers.size() - 1;
    StartCommandBuffer(index);
    return m_CommandBuffers[index];
}

void CommandBufferManager::EndSingleTimeCommands()
{
    vk::SubmitInfo submitInfo = vk::SubmitInfo{};
    Submit(submitInfo, (uint32_t)m_CommandBuffers.size() - 1);
    m_Queue.waitIdle();
}

std::vector<vk::CommandBuffer> CommandBufferManager::AllocateCommandBuffers(uint32_t count, std::string_view cbName)
{
    auto device = m_Context->GetDevice();
    vk::CommandBufferAllocateInfo allocInfo(
        m_CommandPool,
        vk::CommandBufferLevel::ePrimary,
        count
    );
    auto cbs = device.allocateCommandBuffers(allocInfo);

    for (uint32_t i = 0; i < count; ++i)
        m_Context->SetVkObjectName(cbs[i], std::format("CommandBuffer: {}, {}", cbName, i));

    return cbs;
}

vk::CommandBuffer CommandBufferManager::AllocateCommandBuffer(std::string_view cbName)
{
    return AllocateCommandBuffers(1, cbName)[0];
}
}
