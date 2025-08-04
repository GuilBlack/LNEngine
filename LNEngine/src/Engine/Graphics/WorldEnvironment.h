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
    glm::vec3 SunDirection{ 0.0f, -1.0f, 0.0f }; // the direction of the sun light
    float AmbientLight{ 0.03f }; // the ambient light intensity
};
}
