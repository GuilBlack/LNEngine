#include "CommandBufferManager.h"
#include "GfxContext.h"
#include "Core/SafePtr.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"

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
    m_Context->GetDevice().resetFences(m_WaitFences[index]);
    m_CommandBuffers[index].end();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[index];
    m_Queue.submit(submitInfo, m_WaitFences[index]);
}

vk::CommandBuffer CommandBufferManager::BeginSingleTimeCommands()
{
    m_SingleTimeMutex.lock();
    uint32_t index = (uint32_t)m_CommandBuffers.size() - 1;
    StartCommandBuffer(index);
    return m_CommandBuffers[index];
}

void CommandBufferManager::EndSingleTimeCommands()
{
    vk::SubmitInfo submitInfo = vk::SubmitInfo{};
    Submit(submitInfo, (uint32_t)m_CommandBuffers.size() - 1);
    m_Queue.waitIdle();
    m_SingleTimeMutex.unlock();
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

CommandPoolManager::CommandPoolManager(GfxContext* ctx, uint32_t numThreads)
    : m_Context(ctx), m_GraphicsFrameContexts{}, m_GraphicsSingleUseContext{},
    m_TransferSingleUseContext{}, m_ComputeSingleUseContext{}
{
    InitSingleUseContext(m_GraphicsSingleUseContext, numThreads, EQueueFamilyType::Graphics);
    InitSingleUseContext(m_TransferSingleUseContext, numThreads, EQueueFamilyType::Transfer);
    InitSingleUseContext(m_ComputeSingleUseContext, numThreads, EQueueFamilyType::Compute);
    InitFrameContext(numThreads);
}

CommandPoolManager::~CommandPoolManager()
{
    DestroySingleUseContext(m_GraphicsSingleUseContext);
    DestroySingleUseContext(m_TransferSingleUseContext);
    DestroySingleUseContext(m_ComputeSingleUseContext);
    DestroyFrameContext();
}

vk::CommandBuffer CommandPoolManager::BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily)
{
    SingleUseCommandContext* context = ChooseSingleUseContext(queueFamily);

    uint32_t index{};
    {
        const std::lock_guard<std::mutex> lock(context->Mutex);

        auto it = std::find_if(
            context->AreUsed.begin(), context->AreUsed.end(), 
            [](const CommandPoolManager::ThreadIdIndex& item) {
                return item.ThreadId == std::this_thread::get_id();
            }
        );
        if (it != context->AreUsed.end())
            return context->ThreadContexts[it->Index].PrimaryCommandBuffer;

        index = context->AreUnused.back();
        context->AreUnused.pop_back();
        context->AreUsed.push_back({ std::this_thread::get_id(), index });
    }

    vk::CommandBuffer cb = context->ThreadContexts[index].PrimaryCommandBuffer;
    cb.begin(
        vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    return cb;
}

void CommandPoolManager::EndSingleUseCommandBuffer(EQueueFamilyType queueFamily,
    vk::PipelineStageFlags* pipelineStage,
    vk::Semaphore* semaphore
)
{
    SingleUseCommandContext* context = ChooseSingleUseContext(queueFamily);
    uint32_t index{};
    std::vector<CommandPoolManager::ThreadIdIndex>::iterator it;
    {
        const std::lock_guard<std::mutex> lock(context->Mutex);
        it = std::find_if(
            context->AreUsed.begin(), context->AreUsed.end(),
            [](const CommandPoolManager::ThreadIdIndex& item)
            {
                return item.ThreadId == std::this_thread::get_id();
            }
        );
        if (it == context->AreUsed.end())
        {
            LNE_ERROR(std::format(
                "Single use command buffer of type {} is not in use by this thread.", 
                QueueFamilyTypeToString(queueFamily)));
            return;
        }
        index = it->Index;
        vk::CommandBuffer cb = context->ThreadContexts[index].PrimaryCommandBuffer;
        cb.end();
        m_Context->GetDevice().resetFences(context->WaitFences[index]);
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBuffers(cb);
        submitInfo.setPWaitDstStageMask(pipelineStage);
        submitInfo.setPWaitSemaphores(semaphore);
        m_Context->SubmitToQueue(queueFamily, submitInfo, context->WaitFences[index]);
    }
    VK_CHECK(m_Context->GetDevice().waitForFences(context->WaitFences[index], VK_TRUE, UINT64_MAX));
    {
        const std::lock_guard<std::mutex> lock(context->Mutex);
        context->AreUnused.emplace_back(index);
        context->AreUsed.erase(it);
    }
}

void CommandPoolManager::InitSingleUseContext(SingleUseCommandContext& context, 
    uint32_t numThreads, 
    EQueueFamilyType queueFamily)
{
    context.ThreadContexts.reserve(numThreads);
    context.WaitFences.reserve(numThreads);
    context.AreUnused.reserve(numThreads);
    context.AreUsed.reserve(numThreads);

    for (uint32_t i = 0; i < numThreads; ++i)
    {
        ThreadCommandContext threadContext{};

        threadContext.CommandPool = m_Context->CreateCommandPool(m_Context->GetQueueFamilyIndex(queueFamily));
        m_Context->SetVkObjectName(threadContext.CommandPool, 
            std::format("{}SingleUseCommandPool{}", QueueFamilyTypeToString(queueFamily), i));

        threadContext.PrimaryCommandBuffer = AllocateCommandBuffer(
            threadContext.CommandPool, vk::CommandBufferLevel::ePrimary,
            std::format("{}SingleUseCommandBuffer{}", QueueFamilyTypeToString(queueFamily), i));

        context.ThreadContexts.emplace_back(threadContext);
        
        context.WaitFences.emplace_back(m_Context->GetDevice().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
        m_Context->SetVkObjectName(context.WaitFences[i], 
            std::format("{}SingleUseCommandBufferFence{}", QueueFamilyTypeToString(queueFamily), i));
        
        context.AreUnused.emplace_back(i); // index of the command buffers that are unused
    }
}

void CommandPoolManager::DestroySingleUseContext(SingleUseCommandContext& context)
{
    for (int i = 0; i < context.ThreadContexts.size(); ++i)
    {
        auto& threadContext = context.ThreadContexts[i];
        m_Context->GetDevice().destroyCommandPool(threadContext.CommandPool);
        m_Context->GetDevice().destroyFence(context.WaitFences[i]);
    }
}

void CommandPoolManager::InitFrameContext(uint32_t numThreads)
{
    m_GraphicsFrameContexts.resize(m_Context->GetMaxFramesInFlight());
    int index{ 0 };
    for (auto& frameContext : m_GraphicsFrameContexts)
    {
        frameContext.ThreadContexts.resize(numThreads);
        frameContext.WaitFences.resize(numThreads);
        frameContext.AreUsed.reserve(numThreads);
        for (uint32_t j = 0; j < numThreads; ++j)
        {
            auto& threadContext = frameContext.ThreadContexts[j];
            threadContext.CommandPool = m_Context->CreateCommandPool(m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));
            m_Context->SetVkObjectName(threadContext.CommandPool, 
                std::format("GraphicsFrameContext{}CommandPool{}", index, j));

            threadContext.PrimaryCommandBuffer = AllocateCommandBuffer(
                threadContext.CommandPool, vk::CommandBufferLevel::ePrimary,
                std::format("GraphicsFrameContext{}Primary", index++));

            frameContext.WaitFences[j] = m_Context->GetDevice().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
            m_Context->SetVkObjectName(frameContext.WaitFences[j], 
                std::format("GraphicsFrameContext{}Fence{}", index, j));
        }
    }
}

void CommandPoolManager::DestroyFrameContext()
{
    m_Context->GetDevice().waitIdle();
    for (auto& frameContext : m_GraphicsFrameContexts)
    {
        for (int i = 0; i < frameContext.ThreadContexts.size(); ++i)
        {
            auto& threadContext = frameContext.ThreadContexts[i];
            m_Context->GetDevice().destroyCommandPool(threadContext.CommandPool);
            threadContext.SecondaryCommandBuffers.clear();
            threadContext.RenderPasses.clear();
            m_Context->GetDevice().destroyFence(frameContext.WaitFences[i]);
        }
    }
}

vk::CommandBuffer CommandPoolManager::AllocateCommandBuffer(
    vk::CommandPool pool, vk::CommandBufferLevel level, 
    std::string_view cbName /*= "CommandBuffer"*/)
{
    vk::CommandBufferAllocateInfo allocInfo(pool, level, 1);
    vk::CommandBuffer cb = m_Context->GetDevice().allocateCommandBuffers(allocInfo)[0];
    m_Context->SetVkObjectName(cb, cbName);
    return cb;
}

lne::CommandPoolManager::SingleUseCommandContext* CommandPoolManager::ChooseSingleUseContext(EQueueFamilyType queueFamily)
{
    SingleUseCommandContext* context{};
    switch (queueFamily)
    {
    case EQueueFamilyType::Graphics:
        context = &m_GraphicsSingleUseContext;
        break;
    case EQueueFamilyType::Transfer:
        context = &m_TransferSingleUseContext;
        break;
    case EQueueFamilyType::Compute:
        context = &m_ComputeSingleUseContext;
        break;
    default:
        LNE_ASSERT(false, std::format("Invalid queue family type for single use command buffer: {}", QueueFamilyTypeToString(queueFamily)));
    }
    return context;
}

}
