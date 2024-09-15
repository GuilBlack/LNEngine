#pragma once
#include "Engine/Core/Utils/Defines.h"
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/UniformBuffer.h"

namespace lne
{
struct TransformComponent
{
    glm::vec3 Position{};
    glm::vec3 Rotation{};
    glm::vec3 Scale{ 1.0f, 1.0f, 1.0f };

    glm::mat4 GetModelMatrix() const
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, Position);
        model = glm::rotate(model, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, Scale);
        return model;
    }

    glm::mat4 GetRotationMatrix() const
    {
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation = glm::rotate(rotation, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        return rotation;
    }

    glm::vec3 GetForward() const
    {
        glm::vec4 forward = GetRotationMatrix() * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        return glm::normalize(glm::vec3(forward));
    }

    glm::vec3 GetRight() const
    {
        glm::vec4 right = GetRotationMatrix() * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        return glm::normalize(glm::vec3(right));
    }

    glm::vec3 GetUp() const
    {
        glm::vec4 up = GetRotationMatrix() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        return glm::normalize(glm::vec3(up));
    }

    void LookAt(const glm::vec3& target)
    {
        glm::vec3 direction = glm::normalize(target - Position);
        Rotation.x = glm::degrees(asin(-direction.y));
        Rotation.y = glm::degrees(atan2(direction.x, direction.z));
    }

    lne::SafePtr<lne::UniformBufferManager> UniformBuffers;
};

struct CameraComponent
{
    glm::mat4 View;
    glm::mat4 Proj;

    glm::mat4 GetViewProj() const
    {
        return Proj * View;
    }

    void SetPerspective(float fov, float aspect, float nearPlane, float farPlane)
    {
        Proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
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
}
