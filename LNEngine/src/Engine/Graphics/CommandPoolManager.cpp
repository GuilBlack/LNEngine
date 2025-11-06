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
    NukeSingleUseContext(m_GraphicsSingleUseContext);
    NukeSingleUseContext(m_TransferSingleUseContext);
    NukeSingleUseContext(m_ComputeSingleUseContext);
    NukeFrameContext();
}

vk::CommandBuffer CommandPoolManager::BeginOrGetPrimaryFrameCommandBuffer(uint32_t frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    int32_t index;
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
            if (frameContext.ThreadContexts[it->Index].IsPrimaryCommandBufferUsed == false)
            {
                frameContext.ThreadContexts[it->Index].IsPrimaryCommandBufferUsed = true;
                vk::CommandBuffer cb = frameContext.ThreadContexts[it->Index].PrimaryCommandBuffer;
                cb.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
                return cb;
            }
            return frameContext.ThreadContexts[it->Index].PrimaryCommandBuffer;
        }

        index = frameContext.AreUnused.back();
        frameContext.AreUnused.pop_back();
        frameContext.AreUsed.emplace_back(ThreadIdIndex{ std::this_thread::get_id(), index });
    }

    vk::CommandBuffer cb = frameContext.ThreadContexts[index].PrimaryCommandBuffer;
    frameContext.ThreadContexts[index].IsPrimaryCommandBufferUsed = true;
    cb.begin(
        vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    return cb;
}

vk::CommandBuffer CommandPoolManager::BeginRenderPassCommandBuffer(uint32_t frameIndex,
                                                                   Framebuffer* fb /*= nullptr*/)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    vk::CommandBuffer cb;
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

        int32_t index;
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
            vk::CommandBuffer newCb = AllocateCommandBuffer(
                threadContext->CommandPool, vk::CommandBufferLevel::eSecondary);
            threadContext->SecondaryCommandBuffers.emplace_back(newCb);
        }

        cb = threadContext->SecondaryCommandBuffers[threadContext->CurrentSecondaryIndex];
        ++threadContext->CurrentSecondaryIndex;
    }

    vk::CommandBufferInheritanceRenderingInfo renderingInherit{};
    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    vk::CommandBufferBeginInfo beginInfo{};

    if (fb != nullptr)
    {
        renderingInherit = fb->GetInheritanceRenderingInfo();

        vk::StructureChain<
            vk::CommandBufferInheritanceInfo,
            vk::CommandBufferInheritanceRenderingInfo> chain{
                inheritanceInfo, 
                renderingInherit
        };

        beginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue |
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        beginInfo.pInheritanceInfo = &chain.get<vk::CommandBufferInheritanceInfo>();

        cb.begin(beginInfo);
    }
    else
    {
        // secondary outside render pass (compute / transfer / whatever)
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        beginInfo.pInheritanceInfo = &inheritanceInfo; // ok to be mostly empty
        cb.begin(beginInfo);
    }

    return cb;
}

void CommandPoolManager::ResetFrameCommands(uint32_t frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    vk::Device device = m_Context->GetDevice();

    VK_CHECK(device.waitForFences(frameContext.WaitFence, VK_TRUE, UINT64_MAX));
    for (auto& threadContext : frameContext.ThreadContexts)
    {
        device.resetCommandPool(threadContext.CommandPool);
        threadContext.IsPrimaryCommandBufferUsed = false;
        threadContext.CurrentSecondaryIndex = 0;
        threadContext.SecondaryCommandBuffers.clear();
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
FrameCommands CommandPoolManager::EndFrame(uint32_t frameIndex)
{
    FrameCommandContext& frameContext = m_GraphicsFrameContexts[frameIndex];
    FrameCommands fc{};
    for (ThreadIdIndex threadIdIndex : frameContext.AreUsed)
    {
        if (!frameContext.ThreadContexts[threadIdIndex.Index].IsPrimaryCommandBufferUsed)
            continue;
        ThreadCommandContext& threadContext = frameContext.ThreadContexts[threadIdIndex.Index];
        threadContext.PrimaryCommandBuffer.end();
        fc.CommandBuffers.emplace_back(threadContext.PrimaryCommandBuffer);
    }
    fc.Fence = frameContext.WaitFence;
    return fc;
}

vk::CommandBuffer CommandPoolManager::BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily)
{
    SingleUseCommandContext* singleUseContext = ChooseSingleUseContext(queueFamily);
    int32_t index{};
    {
        const std::lock_guard<std::mutex> lock(singleUseContext->Mutex);

        auto it = std::find_if(
            singleUseContext->AreUsed.begin(), singleUseContext->AreUsed.end(), 
            [](const CommandPoolManager::ThreadIdIndex& item) {
                return item.ThreadId == std::this_thread::get_id();
            }
        );
        if (it != singleUseContext->AreUsed.end())
            return singleUseContext->ThreadContexts[it->Index].PrimaryCommandBuffer;

        index = singleUseContext->AreUnused.back();
        singleUseContext->AreUnused.pop_back();
        singleUseContext->AreUsed.push_back({ std::this_thread::get_id(), index });
    }

    vk::CommandBuffer cb = singleUseContext->ThreadContexts[index].PrimaryCommandBuffer;
    cb.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
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
    int32_t index{};
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

void CommandPoolManager::NukeSingleUseContext(SingleUseCommandContext& context)
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
        frameContext.AreUnused.reserve(numThreads);
        frameContext.AreUsed.reserve(numThreads);
        for (uint32_t j = 0; j < numThreads; ++j)
        {
            auto& threadContext = frameContext.ThreadContexts[j];
            threadContext.CommandPool = m_Context->CreateCommandPool(m_Context->GetQueueFamilyIndex(EQueueFamilyType::Graphics));
            m_Context->SetVkObjectName(threadContext.CommandPool, 
                std::format("GraphicsFrameContext{}CommandPool{}", index, j));

            threadContext.PrimaryCommandBuffer = AllocateCommandBuffer(
                threadContext.CommandPool, vk::CommandBufferLevel::ePrimary,
                std::format("GraphicsFrameContext{}Thread{}Primary", index, j));

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
            m_Context->GetDevice().destroyCommandPool(threadContext.CommandPool);
            threadContext.SecondaryCommandBuffers.clear();
        }
        m_Context->GetDevice().destroyFence(frameContext.WaitFence);
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
