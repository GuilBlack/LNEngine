#pragma once
#include "Engine/Graphics/Enums.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/FrameGraph/RenderPass/IRenderPass.h"
#include "Engine/Graphics/Resources/Effect.h"
#include "Engine/Core/DataStructures/FlatHashClasses.h"

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
    SafePtr<Effect>     PassEffect;
};

struct GfxTechniqueState
{
    TransparencyMode::Enum          Transparency = TransparencyMode::eOpaque;
    bool                            DeriveDepthFromTransparency = true;
    DepthMode::Enum                 DepthMode = DepthMode::eReadWrite;
    ECompareOperation               DepthCompareOp = ECompareOperation::LessOrEqual;
    EFillMode                       Fill = EFillMode::Solid;
    ECullMode                       Cull = ECullMode::Back;
};

struct GfxTechniqueDesc
{
    std::string                     Name;
    GfxTechniqueState               TechniqueState{};
    std::vector<PassBindingDesc>    Passes{};
};

class GfxTechnique :
    public RefCountBase
{
public:

    [[nodiscard]] PipelineHandle                                CreateOrGetPipeline(PassID passID, SafePtr<FrameGraph> frameGraph);
    [[nodiscard]] SafePtr<GfxPipeline>                          GetPipeline(PassID passID, PipelineHandle handle);
    [[nodiscard]] SafePtr<Effect>                               GetPassEffect(PassID passID);
    [[nodiscard]] const FlatHashMap<PassID, PassBinding>&       GetPasses() const { return m_Passes; }
    [[nodiscard]] FlatHashMap<PassID, MaterialPassSlot>         AllocateMaterialSlots();

private:
    friend class Renderer;
    std::string                         m_Name;
    std::mutex                          m_PipelineMutex;
    FlatHashMap<PassID, PassBinding>    m_Passes;
    GfxTechniqueState                   m_State;

private:
    GfxTechnique(const GfxTechniqueDesc& desc);
};
}
