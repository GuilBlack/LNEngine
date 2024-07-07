#pragma once

namespace lne
{
#define EVENT_CLASS_METHODS(type)  ~##type##Event() = default; \
                                static EEventType GetStaticType() { return EEventType::type; } \
                                virtual EEventType GetType() const override { return GetStaticType(); } \
                                virtual std::string GetName() const override { return #type; }

enum class EEventType
{
    WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
    MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled,
    KeyPressed, KeyReleased
};

class Event
{
public:
    bool Handled = false;

    virtual ~Event() = default;
    virtual EEventType GetType() const = 0;
    virtual std::string GetName() const = 0;
};
}
