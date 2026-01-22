#pragma once
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/Resources/UniformBuffer.h"

namespace lne
{
struct NameComponent
{
    std::string Name;
};

struct TransformComponent
{
    glm::vec3 Position{};
    glm::quat Rotation{};
    glm::vec3 EulerAngles{};
    glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

    glm::mat4 GetModelMatrix() const
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model *= glm::mat4_cast(Rotation);
        model = glm::scale(model, Scale);
        return model;
    }

    glm::mat4 GetRotationMatrix() const
    {
        return glm::mat4_cast(Rotation);
    }

    glm::vec3 GetForward() const
    {
        return glm::normalize(Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 GetRight() const
    {
        return glm::normalize(Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 GetUp() const
    {
        return glm::normalize(Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void LookAt(const glm::vec3& target)
    {
        glm::vec3 direction = glm::normalize(target - Position);
        Rotation.x = glm::degrees(asin(-direction.y));
        Rotation.y = glm::degrees(atan2(direction.x, direction.z));
    }

    void Rotate(float yaw, float pitch, float roll)
    {
        glm::quat yawRot = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat pitchRot = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat rollRot = glm::angleAxis(glm::radians(roll), glm::vec3(0.0f, 0.0f, 1.0f));
        Rotation = glm::normalize(yawRot * pitchRot * rollRot * Rotation);
        EulerAngles = glm::degrees(glm::eulerAngles(Rotation));
    }

    void Rotate(glm::quat rotation)
    {
        Rotation = rotation * Rotation;
        EulerAngles = glm::degrees(glm::eulerAngles(Rotation));
    }

    void SetEulerAngles(const glm::vec3& eulerAngles)
    {
        EulerAngles = eulerAngles;
        Rotation = glm::quat(glm::radians(EulerAngles));
    }
};

struct CameraComponent
{
    glm::mat4 View{};
    glm::mat4 Proj{};
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };
    ProjectionType Projection{};
    bool IsPrimary{ false };

    glm::mat4 GetViewProj() const
    {
        return Proj * View;
    }

    void SetPerspective(float fov, float aspect, float nearPlane, float farPlane)
    {
        assert(abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);

        float const tanHalfFovy = tan(glm::radians(fov) * 0.5f);

        Proj = glm::mat<4, 4, float, glm::defaultp>(0.0f);
        Proj[0][0] = 1.0f / (aspect * tanHalfFovy);
        Proj[1][1] = 1.0f / (tanHalfFovy);

        // had to modify this to have a reversed Z
        Proj[2][2] = nearPlane / (farPlane - nearPlane);
        Proj[2][3] = -1.0f;
        Proj[3][2] = (farPlane * nearPlane) / (farPlane - nearPlane);
    }

    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        Proj = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    }

    void LookAtCenter(const glm::vec3& position)
    {
        View = glm::lookAt(position, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void LookAt(const glm::vec3& position, const glm::vec3& forward)
    {
        glm::vec3 lookAtPoint = position + forward;
        View = glm::lookAt(position, lookAtPoint, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void UpdateView(const TransformComponent& transform)
    {
        glm::vec3 eye = transform.Position;
        glm::vec3 center = transform.Position + glm::vec3(transform.GetForward());
        View = glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

struct StaticMeshComponent
{
    SafePtr<class StaticMesh> Mesh{};
};

struct LightComponent
{
    LightType::Enum Type{ LightType::ePoint };
    glm::vec3       Color{ 1.0f, 1.0f, 1.0f };
    float           Intensity{ 1.0f };
    float           Range{ 10.0f };
    float           Falloff{ 1.0f };
    float           SpotAngle{ glm::radians(30.0f) };
    bool            CastsShadows{ false }; // TODO: implement shadow casting
};
}
