#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Structs.h"
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Graphics/Resources/Texture.h"

namespace lne
{
class WorldEnvironment : public RefCountBase
{
public:
    SafePtr<Texture>    SkyboxTexture{};
    SafePtr<Texture>    PrefilteredTexture{};
    SafePtr<Texture>    IrradianceTexture{};
    LightGPUData        SunLight{
        LightType::eDirectional,
        glm::vec3(0.0f),
        glm::normalize(glm::vec3(-.5f, -1.0f, 0.5f)),
        glm::vec3(1.0f, 1.0f, 1.0f),
        1.0f,
        0.0f,
        0.0f,
        0.0f
    };
    bool                IsSunEnabled{ true };
    float               AmbientLight{ 0.03f };
};
}
