#include "CommandPoolManager.h"
#include "GfxContext.h"
#include "Core/SafePtr.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/_Defines.h"
#include "Core/ApplicationBase.h"
#include "Graphics/Resources/Texture.h"
#include "Framebuffer.h"

namespace lne
{
CommandPoolManager::CommandPoolManager(GfxContext* ctx, u32 numThreads)
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
    NukeSingleUseContext(m_GraphicsSingleUseContext);
    NukeSingleUseContext(m_TransferSingleUseContext);
    NukeSingleUseContext(m_ComputeSingleUseContext);
    NukeFrameContext();
}

CommandBuffer* CommandPoolManager::BeginOrGetPrimaryFrameCommandBuffer(u32 frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    s32 index;
    {
        std::lock_guard lock(m_GraphicsFrameContexts[frameIndex].Mutex);
        auto it = std::find_if(
            frameContext.AreUsed.begin(), frameContext.AreUsed.end(),
            [](const CommandPoolManager::ThreadIdIndex& item)
            {
                return item.ThreadId == std::this_thread::get_id();
            }
        );
        if (it != frameContext.AreUsed.end())
        {
            auto& threadCtx = frameContext.ThreadContexts[it->Index];
            CommandBuffer* cb = threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
            if (threadCtx.IsPrimaryCommandBufferUsed == false)
            {
                threadCtx.IsPrimaryCommandBufferUsed = true;
                cb->BeginRecording(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
                return cb;
            }
            return threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
        }

        index = frameContext.AreUnused.back();
        frameContext.AreUnused.pop_back();
        frameContext.AreUsed.emplace_back(ThreadIdIndex{ std::this_thread::get_id(), index });
    }
    auto& threadCtx = frameContext.ThreadContexts[index];
    CommandBuffer* cb = threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
    threadCtx.IsPrimaryCommandBufferUsed = true;
    cb->BeginRecording(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    return cb;
}

CommandBuffer* CommandPoolManager::BeginRenderPassCommandBuffer(u32 frameIndex,
                                                                   Framebuffer* fb /*= nullptr*/)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    CommandBuffer* cb = nullptr;
    ThreadCommandContext* threadContext = nullptr;

    {
        std::lock_guard lock(frameContext.Mutex);

        auto it = std::find_if(
            frameContext.AreUsed.begin(), frameContext.AreUsed.end(),
            [](const ThreadIdIndex& item)
            {
                return item.ThreadId == std::this_thread::get_id();
            }
        );

        s32 index;
        if (it == frameContext.AreUsed.end())
        {
            index = frameContext.AreUnused.back();
            frameContext.AreUnused.pop_back();
            frameContext.AreUsed.emplace_back(ThreadIdIndex{ std::this_thread::get_id(), index });
        }
        else
            index = it->Index;

        threadContext = &frameContext.ThreadContexts[index];
        if (threadContext->CurrentSecondaryIndex >= threadContext->SecondaryCommandBuffers.size())
        {
            CommandBufferHandle newCb = threadContext->CommandBuffers.Allocate(
                m_Context, threadContext->CommandPool,
                EQueueFamilyType::Graphics, CommandBuffer::eSecondary, "SecondaryCommandBuffer");
            threadContext->SecondaryCommandBuffers.emplace_back(newCb);
        }

        auto cbHandle = threadContext->SecondaryCommandBuffers[threadContext->CurrentSecondaryIndex];
        CommandBuffer* cb = threadContext->CommandBuffers.Access(cbHandle);
        ++threadContext->CurrentSecondaryIndex;
    }

    vk::CommandBufferInheritanceRenderingInfo renderingInherit{};
    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    vk::CommandBufferBeginInfo beginInfo{};

    if (fb != nullptr)
    {
        renderingInherit = fb->GetInheritanceRenderingInfo();

        cb->BeginRecording(vk::CommandBufferUsageFlagBits::eRenderPassContinue |
                           vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                           inheritanceInfo, renderingInherit);
    }
    else
        cb->BeginRecording(vk::CommandBufferUsageFlagBits::eOneTimeSubmit, inheritanceInfo);

    return cb;
}

void CommandPoolManager::WaitForFrameCommands(u32 frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    VK_CHECK(m_Context->GetDevice().waitForFences(frameContext.WaitFence, VK_TRUE, UINT64_MAX));
}

void CommandPoolManager::ResetFrameCommands(u32 frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    vk::Device device = m_Context->GetDevice();

    for (auto& threadContext : frameContext.ThreadContexts)
    {
        device.resetCommandPool(threadContext.CommandPool);
        CommandBuffer* cb = threadContext.CommandBuffers.Access(threadContext.PrimaryCommandBuffer);
        cb->ClearState();
        threadContext.IsPrimaryCommandBufferUsed = false;
        threadContext.CurrentSecondaryIndex = 0;
        //threadContext.SecondaryCommandBuffers.clear();
        for (auto& secondaryCbHandle : threadContext.SecondaryCommandBuffers)
        {
            CommandBuffer* secondaryCb = threadContext.CommandBuffers.Access(secondaryCbHandle);
            secondaryCb->ClearState();
        }
    }
    device.resetFences(frameContext.WaitFence);

    std::lock_guard lock(frameContext.Mutex);
    while (frameContext.AreUsed.size() > 0)
    {
        ThreadIdIndex threadIdIndex = frameContext.AreUsed.back();
        frameContext.AreUnused.emplace_back(threadIdIndex.Index);
        frameContext.AreUsed.pop_back();
    }
}

// TODO: the management of used primary command buffers shouldn't be done here?
FrameCommands CommandPoolManager::EndFrame(u32 frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    FrameCommands fc{};
    for (ThreadIdIndex threadIdIndex : frameContext.AreUsed)
    {
        if (!frameContext.ThreadContexts[threadIdIndex.Index].IsPrimaryCommandBufferUsed)
            continue;
        ThreadCommandContext& threadCtx = frameContext.ThreadContexts[threadIdIndex.Index];
        CommandBuffer* cb = threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
        cb->EndRecording();
        fc.CommandBuffers.emplace_back(cb->GetVkCommandBuffer());
    }
    fc.Fence = frameContext.WaitFence;
    return fc;
}

CommandBuffer* CommandPoolManager::BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily)
{
    SingleUseCommandContext* singleUseContext = ChooseSingleUseContext(queueFamily);
    s32 index{};
    {
        const std::lock_guard<std::mutex> lock(singleUseContext->Mutex);

        auto it = std::find_if(
            singleUseContext->AreUsed.begin(), singleUseContext->AreUsed.end(), 
            [](const CommandPoolManager::ThreadIdIndex& item) {
                return item.ThreadId == std::this_thread::get_id();
            }
        );
        if (it != singleUseContext->AreUsed.end())
        {
            auto& threadCtx = singleUseContext->ThreadContexts[it->Index];
            return threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
        }

        index = singleUseContext->AreUnused.back();
        singleUseContext->AreUnused.pop_back();
        singleUseContext->AreUsed.push_back({ std::this_thread::get_id(), index });
    }
    auto& threadCtx = singleUseContext->ThreadContexts[index];
    CommandBuffer* cb = threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);

    cb->Reset();
    cb->BeginRecording(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    return cb;
}

void CommandPoolManager::EndSingleUseCommandBuffer(EQueueFamilyType queueFamily,
    vk::PipelineStageFlags* pipelineStage,
    vk::Semaphore* semaphore
)
{
    SingleUseCommandContext* context = ChooseSingleUseContext(queueFamily);
    s32 index{};
    CommandBuffer* cb = nullptr;
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
        auto& threadCtx = context->ThreadContexts[index];
        cb = threadCtx.CommandBuffers.Access(threadCtx.PrimaryCommandBuffer);
        cb->EndRecording();
        m_Context->GetDevice().resetFences(context->WaitFences[index]);
        vk::SubmitInfo submitInfo{};
        vk::CommandBuffer vkCb = cb->GetVkCommandBuffer();
        submitInfo.setCommandBuffers(vkCb);
        submitInfo.setPWaitDstStageMask(pipelineStage);
        submitInfo.setPWaitSemaphores(semaphore);
        m_Context->SubmitToQueue(queueFamily, submitInfo, context->WaitFences[index]);
    }
    VK_CHECK(m_Context->GetDevice().waitForFences(context->WaitFences[index], VK_TRUE, UINT64_MAX));
    {
        const std::lock_guard<std::mutex> lock(context->Mutex);
        context->AreUnused.emplace_back(index);
        context->AreUsed.erase(it);
        cb->ClearState();
    }
}

void CommandPoolManager::InitSingleUseContext(SingleUseCommandContext& context, 
    u32 numThreads, 
    EQueueFamilyType queueFamily)
{
    context.ThreadContexts.reserve(numThreads);
    context.WaitFences.reserve(numThreads);
    context.AreUnused.reserve(numThreads);
    context.AreUsed.reserve(numThreads);

    for (u32 i = 0; i < numThreads; ++i)
    {
        context.ThreadContexts.emplace_back(ThreadCommandContext{});
        auto& threadContext = context.ThreadContexts.back();

        threadContext.CommandPool = m_Context->CreateCommandPool(m_Context->GetQueueFamilyIndex(queueFamily));
        m_Context->SetVkObjectName(threadContext.CommandPool, 
            std::format("{}SingleUseCommandPool{}", QueueFamilyTypeToString(queueFamily), i));

        threadContext.PrimaryCommandBuffer = threadContext.CommandBuffers.Allocate(
            m_Context, threadContext.CommandPool,
            queueFamily, CommandBuffer::ePrimary,
            std::format("{}SingleUseCommandBuffer{}", QueueFamilyTypeToString(queueFamily), i)
        );

        
        context.WaitFences.emplace_back(m_Context->GetDevice().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
        m_Context->SetVkObjectName(context.WaitFences[i], 
            std::format("{}SingleUseCommandBufferFence{}", QueueFamilyTypeToString(queueFamily), i));
        
        context.AreUnused.emplace_back(i); // index of the command buffers that are unused
    }
}

void CommandPoolManager::NukeSingleUseContext(SingleUseCommandContext& context)
{
    for (int i = 0; i < context.ThreadContexts.size(); ++i)
    {
        auto& threadContext = context.ThreadContexts[i];
        threadContext.CommandBuffers.Clear();
        m_Context->GetDevice().destroyCommandPool(threadContext.CommandPool);
        m_Context->GetDevice().destroyFence(context.WaitFences[i]);
    }
}

void CommandPoolManager::InitFrameContext(u32 numThreads)
{
    m_GraphicsFrameContexts.resize(m_Context->GetMaxFramesInFlight());
    int index{ 0 };
    for (auto& frameContext : m_GraphicsFrameContexts)
    {
        frameContext.ThreadContexts.resize(numThreads);
        frameContext.AreUnused.reserve(numThreads);
        frameContext.AreUsed.reserve(numThreads);
        for (u32 j = 0; j < numThreads; ++j)
        {
            auto& threadContext = frameContext.ThreadContexts[j];
            threadContext.CommandPool = m_Context->CreateCommandPool(m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));
            m_Context->SetVkObjectName(threadContext.CommandPool, 
                std::format("GraphicsFrameContext{}CommandPool{}", index, j));

            threadContext.PrimaryCommandBuffer = threadContext.CommandBuffers.Allocate(
                m_Context, threadContext.CommandPool,
                EQueueFamilyType::Graphics, CommandBuffer::ePrimary,
                std::format("GraphicsFrameContext{}Thread{}Primary", index, j)
            );

            frameContext.AreUnused.emplace_back(j);
        }
        frameContext.WaitFence = m_Context->GetDevice().createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        m_Context->SetVkObjectName(frameContext.WaitFence,
            std::format("GraphicsFrameContext{}Fence", index++));
    }
}

void CommandPoolManager::NukeFrameContext()
{
    m_Context->GetDevice().waitIdle();
    for (auto& frameContext : m_GraphicsFrameContexts)
    {
        for (int i = 0; i < frameContext.ThreadContexts.size(); ++i)
        {
            auto& threadContext = frameContext.ThreadContexts[i];
            threadContext.CommandBuffers.Clear();
            m_Context->GetDevice().destroyCommandPool(threadContext.CommandPool);
            threadContext.SecondaryCommandBuffers.clear();
        }
        m_Context->GetDevice().destroyFence(frameContext.WaitFence);
    }
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
