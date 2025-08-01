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
    SafePtr<Texture> SkyboxTexture{}; // the actual environment map texture which is the radiance texture
    SafePtr<Texture> PrefilteredTexture{}; // pre-filtered radiance texture used for specular reflections
    SafePtr<Texture> IrradianceTexture{}; // pre-filtered irradiance texture used for diffuse reflections
};
}
