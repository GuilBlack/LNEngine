#pragma once
#include "ECS/Archetype.h"

namespace lne
{
struct TransformComponent;
struct StaticMeshComponent;

class ICustomDrawStaticMeshesAdder
{
public:
    virtual ~ICustomDrawStaticMeshesAdder() = default;

    virtual void AddStaticMeshes(ComponentView<TransformComponent, StaticMeshComponent>&) = 0;
};

struct LightComponent;
class ILightAdder
{
public:
    virtual ~ILightAdder() = default;
    virtual void AddLights(ComponentView<TransformComponent, LightComponent>&) = 0;
};
}
