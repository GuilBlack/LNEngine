#pragma once
#include "Engine/Graphics/Enums.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/FrameGraph/RenderPass/IRenderPass.h"
#include "Engine/Graphics/Resources/Effect.h"

namespace lne
{
class Effect;
class GfxPipeline;
class FrameGraph;
struct PassBindingDesc
{
    std::string         PassName;
    SafePtr<Effect>     PassEffect;
};
struct PassBinding
{
    PassID              PassId;
    PipelineHandle      PipelineHandle{};
    SafePtr<Effect>     PassEffect;
};

class GfxTechnique :
    public RefCountBase
{
public:
    struct State
    {
        TransparencyMode::Enum          Transparency = TransparencyMode::eOpaque;
        bool                            DeriveDepthFromTransparency = true;
        DepthMode::Enum                 DepthMode = DepthMode::eReadWrite;
        ECompareOperation               DepthCompareOp = ECompareOperation::LessOrEqual;
        EFillMode                       Fill = EFillMode::Solid;
        ECullMode                       Cull = ECullMode::Back;
    };
    struct Desc
    {
        std::string                     Name;
        State                           TechniqueState{};
        std::vector<PassBindingDesc>    Passes{};
    };

public:
    GfxTechnique(const Desc& desc);

    [[nodiscard]] PipelineHandle                                CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph);
    [[nodiscard]] SafePtr<GfxPipeline>                          GetPipeline(PassID passID);
    [[nodiscard]] SafePtr<Effect>                               GetPassEffect(PassID passID);
    [[nodiscard]] const std::vector<PassBinding>&               GetPasses() const { return m_Passes; }
    [[nodiscard]] std::vector<MaterialPassSlot>                 AllocateMaterialSlots();

private:
    std::string                 m_Name;
    std::mutex                  m_PipelineMutex;
    std::vector<PassBinding>    m_Passes;
    State                       m_State;
};
}
