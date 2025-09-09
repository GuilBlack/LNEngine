#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Resources/Pipeline.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

namespace lne
{
class GfxContext;
class Shader;
class StorageBuffer;

struct PipelineHandle
{
    uint64_t H1 = 0; // used for indexing
    uint64_t H2 = 0; // verification tag
    bool operator==(const PipelineHandle& o) const { return H1 == o.H1 && H2 == o.H2; }
    bool operator!=(const PipelineHandle& o) const { return !(*this == o); }
};

inline PipelineHandle MakeHandle(const lne::GraphicsPipelineDescV2& d)
{
    // arbitrary large primes
    size_t s1 = 0x41788a801d56c693ull; // A
    size_t s2 = 0x72ec9023ea2a1533ull; // B

    auto mix = [&](size_t& s)
        {
            lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.CullMode));
            lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.Fill));
            lne::GlobalUtils::HashCombine(s, static_cast<size_t>(d.TransparencyMode));
            lne::GlobalUtils::HashCombine(s, static_cast<size_t>(d.DeriveDepthFromTransparency));
            if (!d.DeriveDepthFromTransparency)
            {
                lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.DepthMode));
                lne::GlobalUtils::HashCombine(s, lne::GlobalUtils::HashEnum(d.DepthCompareOp));
            }
        };

    mix(s1);
    mix(s2);

    PipelineHandle out;
    out.H1 = static_cast<uint64_t>(s1);
    out.H2 = static_cast<uint64_t>(s2);
    return out;
}
}

namespace boost
{
template<>
struct hash<lne::PipelineHandle>
{
    std::size_t operator()(const lne::PipelineHandle& p) const
    {
        size_t seed = 0;
        hash_combine(seed, p.H1);
        hash_combine(seed, p.H2);
        return seed;
    }
};
}

namespace lne
{
struct BankItem
{
    std::vector<SafePtr<StorageBuffer>> FrameBuffer{};
    uint32_t                            ElementSize{ 0 };
};

struct MaterialBank
{
    std::vector<BankItem>           Items;
    std::vector<uint32_t>           FreeSlots;
    uint32_t                        Count;
    std::vector<vk::DescriptorSet>  FrameDescSets;
};

class Effect : public RefCountBase
{
public:
    ~Effect();

    /**
     * Create or get a pipeline based on the given description. If a pipeline with the same description already exists, it will be returned.
     * @param pipelineDesc: Description of the pipeline to create or get.
     * @return A handle to the created or existing pipeline.
     */
    [[nodiscard]] PipelineHandle        CreateOrGetPipeline(GraphicsPipelineDescV2& pipelineDesc);

    /**
     * Get a pipeline by its hash. Returns nullptr if not found.
     * @param handle: The handle of the pipeline to get.
     * @return A SafePtr to the GfxPipeline if found, otherwise nullptr.
     */
    [[nodiscard]] SafePtr<GfxPipeline>  GetPipeline(PipelineHandle handle);
    [[nodiscard]] vk::DescriptorSet     GetFrameDescriptorSet(uint32_t frameIndex) const
    {
        return m_Bank.FrameDescSets[frameIndex];
    }

    [[nodiscard]] MaterialSlot          AllocateMaterialSlot();
    void                                FreeMaterialSlot(MaterialSlot slot);

    [[nodiscard]] SafePtr<Shader>       GetShader() const { return m_Shader; }
    virtual std::string_view            GetDebugName() const override { return m_Name; }

private:
    friend class Renderer;
    friend class MaterialV2;
    using PipelineCache = FlatHashMap<PipelineHandle, SafePtr<GfxPipeline>>;
    std::string                         m_Name;
    SafePtr<GfxContext>                 m_Context;
    SafePtr<Shader>                     m_Shader;
    MaterialBank                        m_Bank{};
    std::mutex                          m_PipelinesMutex{};
    PipelineCache                       m_Pipelines{};
    std::mutex                          m_SlotAllocMutex{};
    uint32_t                            m_DirtyFrames{};

private:
    Effect(SafePtr<GfxContext> context, const std::string& shaderPath);

    void                                GrowFreeSlots();
    void                                GrowBank(vk::CommandBuffer cmdBuffer, uint32_t currentFrameInFLight);
    void                                CopyMaterialDataToBuffer(vk::CommandBuffer cmdBuffer,
                                                                 uint32_t currentFrameInFlight,
                                                                 MaterialSlot matSlot,
                                                                 uint32_t binding,
                                                                 void* data);
};
}

