#pragma once

namespace lne
{
class Layer
{
public:
    Layer(const std::string& name = "Layer")
        : m_DebugName(name) {}
    virtual ~Layer() = default;

    virtual void OnAttach() = 0;
    virtual void OnDetach() = 0;
    virtual void OnUpdate(float deltaTime) = 0;
    virtual void OnImGuiRender() = 0;

    inline const std::string& GetName() const { return m_DebugName; }

protected:
    virtual void BindEventCallbacks(int32_t priority) {}

    friend class ApplicationBase;
private:
    std::string m_DebugName;
};
}

// add ostream operator for Layer
std::ostream& operator<<(std::ostream& os, const lne::Layer& layer);
