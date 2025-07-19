#pragma once
#include "Enums.h"
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
    [[nodiscard]] vk::Queue GetQueue() const { return m_Queue; }
    void StartCommandBuffer(uint32_t index);

    void Submit(vk::SubmitInfo& submitInfo, uint32_t index = UINT32_MAX);

    vk::CommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands();

private:
    GfxContext* m_Context;
    vk::Queue m_Queue;
    vk::CommandPool m_CommandPool;
    std::vector<vk::CommandBuffer> m_CommandBuffers;

    std::mutex m_SingleTimeMutex;
    std::vector<vk::Fence> m_WaitFences;

    uint32_t m_CurrentBufferIndex{ 0 };

private:
    [[nodiscard]] std::vector<vk::CommandBuffer> AllocateCommandBuffers(uint32_t count, std::string_view cbName);
    [[nodiscard]] vk::CommandBuffer AllocateCommandBuffer(std::string_view cbName);
};

class CommandPoolManager
{
public:
    CommandPoolManager(GfxContext* ctx, uint32_t numThreads);
    ~CommandPoolManager();

    [[nodiscard]] vk::CommandBuffer BeginOrGetSingleUseCommandBuffer(EQueueFamilyType queueFamily);

    void EndSingleUseCommandBuffer(
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

    struct FrameCommandContext
    {
        std::vector<ThreadCommandContext>           ThreadContexts{};
        std::vector<vk::Fence>                      WaitFences{};
        std::vector<std::thread::id>                AreUsed{}; // which command buffers are currently in use
        std::mutex                                  Mutex; // to protect access to the command buffers

        // default ctor
        FrameCommandContext() = default;

        // delete copy
        FrameCommandContext(const FrameCommandContext&) = delete;
        FrameCommandContext& operator=(const FrameCommandContext&) = delete;
        
        FrameCommandContext(FrameCommandContext&& other) noexcept
            : ThreadContexts(std::move(other.ThreadContexts))
            , WaitFences(std::move(other.WaitFences))
            , AreUsed(std::move(other.AreUsed))
            , Mutex()    // fresh, unlocked mutex
        {}

        FrameCommandContext& operator=(FrameCommandContext&& other) noexcept
        {
            if (this != &other)
            {
                ThreadContexts = std::move(other.ThreadContexts);
                WaitFences = std::move(other.WaitFences);
                AreUsed = std::move(other.AreUsed);
                // Mutex stays as a new one
            }
            return *this;
        }

    };
    struct ThreadIdIndex
    {
        std::thread::id ThreadId{};
        uint32_t Index{ 0 }; // index in the command buffer array

        bool operator ==(const std::thread::id& threadId) const
        {
            return ThreadId == threadId;
        }

    };
    struct SingleUseCommandContext
    {
        std::vector<ThreadCommandContext>   ThreadContexts{};
        std::vector<vk::Fence>              WaitFences{};
        std::vector<uint32_t>               AreUnused{}; // which command buffers are currently unused
        std::vector<ThreadIdIndex>               AreUsed{}; // which command buffers are currently in use
        std::mutex                          Mutex; // to protect access to the command buffers
    };

    GfxContext* m_Context;

    std::vector<FrameCommandContext>    m_GraphicsFrameContexts{};
    SingleUseCommandContext             m_GraphicsSingleUseContext{};
    SingleUseCommandContext             m_TransferSingleUseContext{};
    SingleUseCommandContext             m_ComputeSingleUseContext{};

private:
    void InitSingleUseContext(
        SingleUseCommandContext& context, uint32_t numThreads, EQueueFamilyType queueFamily);
    void DestroySingleUseContext(SingleUseCommandContext& context);
    void InitFrameContext(uint32_t numThreads);
    void DestroyFrameContext();

    [[nodiscard]] vk::CommandBuffer         AllocateCommandBuffer(
        vk::CommandPool pool, vk::CommandBufferLevel level,
        std::string_view cbName = "CommandBuffer");

    [[nodiscard]] SingleUseCommandContext* ChooseSingleUseContext(
        EQueueFamilyType queueFamily);
};

}
