#pragma once
#include "Enums.h"
#include "Engine/Core/SafePtr.h"

namespace lne
{
class GfxContext;
struct FrameCommands
{
    std::vector<vk::CommandBuffer>  CommandBuffers;
    vk::Fence                       Fence;
};
class CommandPoolManager
{
public:
    CommandPoolManager(GfxContext* ctx, uint32_t numThreads);
    ~CommandPoolManager();

    vk::CommandBuffer           BeginOrGetPrimaryFrameCommandBuffer(uint32_t frameIndex);
    vk::CommandBuffer           GetRenderPassCommandBuffer(uint32_t frameIndex);
    void                        ResetFrameCommands(uint32_t frameIndex);

    [[nodiscard]] FrameCommands EndFrame(uint32_t frameIndex);

    [[nodiscard]] vk::CommandBuffer                 BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily);

    void                                            EndSingleUseCommandBuffer(
        EQueueFamilyType queueFamily, 
        vk::PipelineStageFlags* pipelineStage = nullptr, 
        vk::Semaphore* semaphore = nullptr);

private:
    struct ThreadCommandContext
    {
        vk::CommandPool                 CommandPool{};
        vk::CommandBuffer               PrimaryCommandBuffer{};
        bool                            IsPrimaryCommandBufferUsed{ false };
        std::vector<vk::CommandBuffer>  SecondaryCommandBuffers{}; // associated with render passes
        std::vector<uint64_t>           RenderPasses{}; // should clear it after each frame. 0 = no render pass
    };

    struct ThreadIdIndex
    {
        std::thread::id ThreadId{};
        int32_t Index{ -1 }; // index in the command buffer array

        bool operator ==(const std::thread::id& threadId) const
        {
            return ThreadId == threadId;
        }

    };
    struct FrameCommandContext
    {
        std::vector<ThreadCommandContext>           ThreadContexts{};
        vk::Fence                                   WaitFence{};
        std::vector<int32_t>                        AreUnused{};
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
        std::vector<uint32_t>                   AreUnused{}; // which command buffers are currently unused
        std::vector<ThreadIdIndex>              AreUsed{}; // which command buffers are currently in use
        std::mutex                              Mutex; // to protect access to the command buffers
    };

    GfxContext* m_Context;

    std::vector<FrameCommandContext>    m_GraphicsFrameContexts{};
    SingleUseCommandContext             m_GraphicsSingleUseContext{};
    SingleUseCommandContext             m_TransferSingleUseContext{};
    SingleUseCommandContext             m_ComputeSingleUseContext{};

private:
    void InitSingleUseContext(
        SingleUseCommandContext& context, uint32_t numThreads, EQueueFamilyType queueFamily);
    void NukeSingleUseContext(SingleUseCommandContext& context);
    void InitFrameContext(uint32_t numThreads);
    void NukeFrameContext();

    [[nodiscard]] vk::CommandBuffer         AllocateCommandBuffer(
        vk::CommandPool pool, vk::CommandBufferLevel level,
        std::string_view cbName = "CommandBuffer");

    [[nodiscard]] SingleUseCommandContext* ChooseSingleUseContext(
        EQueueFamilyType queueFamily);
};

}
