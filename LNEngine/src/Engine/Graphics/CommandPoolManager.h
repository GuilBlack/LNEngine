#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/DataStructures/ObjectPool.h"
#include "CommandBuffer.h"

namespace lne
{
class GfxContext;
class Framebuffer;
using CommandBufferHandle = ObjectPoolHandle;

struct FrameCommands
{
    std::vector<vk::CommandBuffer>  CommandBuffers;
    vk::Fence                       Fence;
};
class CommandPoolManager
{
public:
    CommandPoolManager(GfxContext* ctx, u32 numThreads);
    ~CommandPoolManager();

    CommandBuffer*                  BeginOrGetPrimaryFrameCommandBuffer(u32 frameIndex);

    /**
     * Begins a render pass command buffer.
     * The frame buffer can be nullptr if no graphics render pass is needed.
     * You must manually end the command buffer after use.
     * @param frameIndex The index of the current frame in flight.
     * @param fb The framebuffer to use for the render pass.
     */
    CommandBuffer*                  BeginRenderPassCommandBuffer(u32 frameIndex,
                                                                 Framebuffer* fb = nullptr);
    void                            WaitForFrameCommands(u32 frameIndex);
    void                            ResetFrameCommands(u32 frameIndex);

    [[nodiscard]] FrameCommands     EndFrame(u32 frameIndex);

    [[nodiscard]] CommandBuffer*    BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily);

    void                            EndSingleUseCommandBuffer(
        EQueueFamilyType queueFamily, 
        vk::PipelineStageFlags* pipelineStage = nullptr, 
        vk::Semaphore* semaphore = nullptr);

private:
    struct ThreadCommandContext
    {
        vk::CommandPool                     CommandPool{};
        ObjectPool<CommandBuffer>           CommandBuffers{};
        CommandBufferHandle                 PrimaryCommandBuffer{};
        bool                                IsPrimaryCommandBufferUsed{ false };
        std::vector<CommandBufferHandle>    SecondaryCommandBuffers{}; // associated with render passes
        u32                                 CurrentSecondaryIndex{ 0 };
    };

    struct ThreadIdIndex
    {
        std::thread::id                 ThreadId{};
        s32                             Index{ -1 }; // index in the command buffer array

        bool operator ==(const std::thread::id& threadId) const
        {
            return ThreadId == threadId;
        }

    };
    struct FrameCommandContext
    {
        std::vector<ThreadCommandContext>           ThreadContexts{};
        vk::Fence                                   WaitFence{};
        std::vector<s32>                            AreUnused{};
        std::vector<ThreadIdIndex>                  AreUsed{}; // which command buffers are currently in use
        std::mutex                                  Mutex; // to protect access to the command buffers

        // default ctor
        FrameCommandContext() = default;

        // delete copy
        FrameCommandContext(const FrameCommandContext&) = delete;
        FrameCommandContext& operator=(const FrameCommandContext&) = delete;
        
        FrameCommandContext(FrameCommandContext&& other) noexcept
            : ThreadContexts(std::move(other.ThreadContexts))
            , WaitFence(other.WaitFence)
            , AreUnused(std::move(other.AreUnused))
            , AreUsed(std::move(other.AreUsed))
            , Mutex()    // fresh, unlocked mutex
        {}

        FrameCommandContext& operator=(FrameCommandContext&& other) noexcept
        {
            if (this != &other)
            {
                ThreadContexts = std::move(other.ThreadContexts);
                WaitFence = std::move(other.WaitFence);
                AreUnused = std::move(other.AreUnused);
                AreUsed = std::move(other.AreUsed);
                // Mutex stays as a new one
            }
            return *this;
        }
    };

    struct SingleUseCommandContext
    {
        std::vector<ThreadCommandContext>       ThreadContexts{};
        std::vector<vk::Fence>                  WaitFences{};
        std::vector<u32>                        AreUnused{}; // which command buffers are currently unused
        std::vector<ThreadIdIndex>              AreUsed{}; // which command buffers are currently in use
        std::mutex                              Mutex; // to protect access to the command buffers
    };

    GfxContext*                         m_Context;
    ObjectPool<CommandBuffer>           m_CommandBufferPool{};
    std::mutex                          m_CommandBufferPoolMutex{};

    std::vector<FrameCommandContext>    m_GraphicsFrameContexts{};
    SingleUseCommandContext             m_GraphicsSingleUseContext{};
    SingleUseCommandContext             m_TransferSingleUseContext{};
    SingleUseCommandContext             m_ComputeSingleUseContext{};

private:
    void                                    InitSingleUseContext(SingleUseCommandContext& context,
                                                                 u32 numThreads, 
                                                                 EQueueFamilyType queueFamily);
    void                                    NukeSingleUseContext(SingleUseCommandContext& context);

    void                                    InitFrameContext(u32 numThreads);
    void                                    NukeFrameContext();

    [[nodiscard]] SingleUseCommandContext* ChooseSingleUseContext(
        EQueueFamilyType queueFamily);
};

}
