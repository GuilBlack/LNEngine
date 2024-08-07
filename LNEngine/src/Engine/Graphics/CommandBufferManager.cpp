#include "CommandBufferManager.h"
#include "GfxContext.h"
#include "Core/SafePtr.h"

namespace lne
{
CommandBufferManager::CommandBufferManager(SafePtr<GfxContext> ctx, uint32_t count, EQueueFamilyType queueType)
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
        m_Context->SetVkObjectName(fence, vk::ObjectType::eFence, "Swapchain Fence");
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

void CommandBufferManager::StartCommandBuffer(uint32_t index)
{
    m_CurrentImageIndex = index;
    auto device = m_Context->GetDevice();
    VK_CHECK(device.waitForFences(m_WaitFences[m_CurrentImageIndex], VK_TRUE, UINT64_MAX));

    m_CommandBuffers[m_CurrentImageIndex].reset(vk::CommandBufferResetFlagBits::eReleaseResources);
    m_CommandBuffers[m_CurrentImageIndex].begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
}

void CommandBufferManager::Submit(const vk::SubmitInfo& submitInfo)
{
    m_Context->GetDevice().resetFences(m_WaitFences[m_CurrentImageIndex]);
    m_CommandBuffers[m_CurrentImageIndex].end();
    m_Queue.submit(submitInfo, m_WaitFences[m_CurrentImageIndex]);
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
        m_Context->SetVkObjectName(cbs[i], vk::CommandBuffer::objectType, std::format("CommandBuffer: {}, {}", cbName, i));

    return cbs;
}

vk::CommandBuffer CommandBufferManager::AllocateCommandBuffer(std::string_view cbName)
{
    return AllocateCommandBuffers(1, cbName)[0];
}
}
