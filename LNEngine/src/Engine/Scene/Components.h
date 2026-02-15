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

    /**
     * elem[0] = near top left    | elem[1] = near top right
     * elem[2] = near bottom left | elem[3] = near bottom right
     * elem[4] = far top left     | elem[5] = far top right
     * elem[6] = far bottom left  | elem[7] = far bottom right
     */
    std::array<glm::vec3, 8> GetFrustumCorners()
    {
        if (Projection == ProjectionType::Perspective)
            return GetPerspectiveFrustumCorners();

        return GetOrthographicFrustumCorners();
    }
private:
    std::array<glm::vec3, 8> GetPerspectiveFrustumCorners()
    {
        std::array<glm::vec4, 8> inter{};
        std::array<glm::vec3, 8> r{};

        // my depth is reversed so it's normal that near = 1 and far = 0;
        glm::mat4 invVP = glm::inverse(Proj * View);
        inter[0] = invVP * glm::vec4{ -1.0f,  1.0f,  1.0f, 1.0f }; // near top left
        inter[1] = invVP * glm::vec4{  1.0f,  1.0f,  1.0f, 1.0f }; // near top right
        inter[2] = invVP * glm::vec4{ -1.0f, -1.0f,  1.0f, 1.0f }; // near bottom left
        inter[3] = invVP * glm::vec4{  1.0f, -1.0f,  1.0f, 1.0f }; // near bottom right
        inter[4] = invVP * glm::vec4{ -1.0f,  1.0f,  0.0f, 1.0f }; // far top left
        inter[5] = invVP * glm::vec4{  1.0f,  1.0f,  0.0f, 1.0f }; // far top right
        inter[6] = invVP * glm::vec4{ -1.0f, -1.0f,  0.0f, 1.0f }; // far bottom left
        inter[7] = invVP * glm::vec4{  1.0f, -1.0f,  0.0f, 1.0f }; // far bottom right

        for (u32 i = 0; i < 8; ++i)
        {
            inter[i] /= inter[i].w;
            r[i] = glm::vec3(inter[i]);
        }

        return r;
        // code that does the same thing but without the inv proj matrix  directly
        //float tanHalfFovx = 1.0f / Proj[0][0];
        //float tanHalfFovy = 1.0f / Proj[1][1];
        //// solve system of equations where:
        //// Proj[2][2] = nearPlane / (farPlane - nearPlane);
        //// Proj[3][2] = (farPlane * nearPlane) / (farPlane - nearPlane);
        //float nearPlane = Proj[3][2] / (Proj[2][2] + 1.0f);
        //float farPlane = Proj[3][2] / Proj[2][2];
        //float yNearHalfLength = tanHalfFovy * nearPlane;
        //float xNearHalfLength = tanHalfFovx * nearPlane;
        //float yFarHalfLength = tanHalfFovy * farPlane;
        //float xFarHalfLength = tanHalfFovx * farPlane;
        //glm::mat4 camTransform = glm::inverse(View);
        //inter[0] = camTransform * glm::vec4{ -xNearHalfLength,  yNearHalfLength,  -nearPlane, 1.0f };
        //inter[1] = camTransform * glm::vec4{  xNearHalfLength,  yNearHalfLength,  -nearPlane, 1.0f };
        //inter[2] = camTransform * glm::vec4{ -xNearHalfLength, -yNearHalfLength,  -nearPlane, 1.0f };
        //inter[3] = camTransform * glm::vec4{  xNearHalfLength, -yNearHalfLength,  -nearPlane, 1.0f };
        //inter[4] = camTransform * glm::vec4{ -xFarHalfLength,  yFarHalfLength,  -farPlane, 1.0f };
        //inter[5] = camTransform * glm::vec4{  xFarHalfLength,  yFarHalfLength,  -farPlane, 1.0f };
        //inter[6] = camTransform * glm::vec4{ -xFarHalfLength, -yFarHalfLength,  -farPlane, 1.0f };
        //inter[7] = camTransform * glm::vec4{  xFarHalfLength, -yFarHalfLength,  -farPlane, 1.0f };
        //r[0] = glm::vec3(inter[0].x, inter[0].y, inter[0].z);
        //r[1] = glm::vec3(inter[1].x, inter[1].y, inter[1].z);
        //r[2] = glm::vec3(inter[2].x, inter[2].y, inter[2].z);
        //r[3] = glm::vec3(inter[3].x, inter[3].y, inter[3].z);
        //r[4] = glm::vec3(inter[4].x, inter[4].y, inter[4].z);
        //r[5] = glm::vec3(inter[5].x, inter[5].y, inter[5].z);
        //r[6] = glm::vec3(inter[6].x, inter[6].y, inter[6].z);
        //r[7] = glm::vec3(inter[7].x, inter[7].y, inter[7].z);
        //return r;
    }

    // TODO: support orthographic projection
    std::array<glm::vec3, 8> GetOrthographicFrustumCorners()
    {
        return {};
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
