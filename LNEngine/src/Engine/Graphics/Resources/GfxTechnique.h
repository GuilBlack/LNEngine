#pragma once
#include "Engine/Graphics/Enums.h"
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
    PassID              PassName;
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

    PipelineHandle CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph);
    SafePtr<GfxPipeline> GetPipeline(PassID passID, PipelineHandle handle);

private:
    std::string                 m_Name;
    std::vector<PassBinding>    m_Passes;
    State                       m_State;
};
}
