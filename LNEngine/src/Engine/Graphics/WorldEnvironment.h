#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/Utils/Defines.h"
#include "Texture.h"

namespace lne
{
class WorldEnvironment : public RefCountBase
{
public:
    SafePtr<Texture> RadianceTexture{};
    SafePtr<Texture> IrradianceTexture{};
};
}
