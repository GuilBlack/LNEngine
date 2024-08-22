#pragma once
#include "Engine/Core/SafePtr.h"
#include "Engine/Graphics/UniformBuffer.h"
#include "Structs.h"

namespace lne
{
class Material : public RefCountBase
{
public:
    MOVABLE_ONLY(Material);
    Material(SafePtr<class GfxPipeline> pipeline);
    ~Material();

    SafePtr<class GfxPipeline> GetPipeline() const { return m_Pipeline; }

    void Set(const std::string& name, const glm::vec4& value);

private:
    SafePtr<class GfxPipeline> m_Pipeline;
    std::unordered_map<std::string, UniformElement> m_MaterialConstants;
    std::map<uint32_t, UniformBuffer> m_UniformBuffers;

    friend class Renderer;

private:
    template<typename T> requires std::is_trivially_copyable_v<T>
    void Set(const std::string& name, const T& value)
    {
        if (m_MaterialConstants.contains(name) == false)
            return;
        auto& matConst = m_MaterialConstants.at(name);
        if (matConst.Size == sizeof(T))
            SetUniformBuffer(m_MaterialConstants.at(name).BindingIndex, &value, sizeof(T), matConst.Offset);
    }
    void SetUniformBuffer(uint32_t binding, const void* data, uint32_t size, uint32_t offset = 0);
};
}
