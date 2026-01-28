#pragma once
#include "Engine/Graphics/Enums.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/FrameGraph/RenderPass/RenderPass.h"
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

    bool IsValid() const;
    ShaderDomain::Enum GetShaderDomain() const;
};

class GfxTechnique :
    public RefCountBase
{
public:

    [[nodiscard]] PipelineHandle                                CreateOrGetPipeline(PassID passId, SafePtr<FrameGraph> frameGraph);
    [[nodiscard]] SafePtr<GfxPipeline>                          GetPipeline(PassID passId, PipelineHandle handle);
    [[nodiscard]] SafePtr<Effect>                               GetPassEffect(PassID passId);
    
    [[nodiscard]] bool                                          ContainsPass(PassID passId) const { return m_Passes.contains(passId); }
    [[nodiscard]] const std::string&                            GetName() const { return m_Name; }
    [[nodiscard]] const GfxTechniqueState&                      GetTechniqueState() const { return m_State; }

    [[nodiscard]] const FlatHashMap<PassID, PassBinding>&       GetPasses() const { return m_Passes; }
    [[nodiscard]] FlatHashMap<PassID, MaterialPassSlot>         AllocateMaterialSlots();

    [[nodiscard]] ShaderDomain::Enum                           GetShaderDomain() const { return m_ShaderDomain; }

private:
    friend class Renderer;
    std::string                         m_Name;
    std::mutex                          m_PassMutex;
    FlatHashMap<PassID, PassBinding>    m_Passes;
    GfxTechniqueState                   m_State;
    ShaderDomain::Enum                  m_ShaderDomain { ShaderDomain::eUnknown };

private:
    GfxTechnique(const GfxTechniqueDesc& desc);
};
}
