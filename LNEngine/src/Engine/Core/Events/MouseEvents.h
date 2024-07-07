#pragma once
#include "Events.h"

namespace lne
{
class MouseButtonPressedEvent : public Event
{
public:
    MouseButtonPressedEvent(MouseButton button)
        : m_Button{ button }
    {}
    MouseButton GetButton() const { return m_Button; }

    EVENT_CLASS_METHODS(MouseButtonPressed)

private:
    MouseButton m_Button;
};

class MouseButtonReleasedEvent : public Event
{
public:
    MouseButtonReleasedEvent(MouseButton button)
        : m_Button{ button }
    {}
    MouseButton GetButton() const { return m_Button; }

    EVENT_CLASS_METHODS(MouseButtonReleased)
private:
    MouseButton m_Button;
};

class MouseMovedEvent : public Event
{
public:
    MouseMovedEvent(float x, float y)
        : m_X{ x }, m_Y{ y }
    {}

    float GetX() const { return m_X; }
    float GetY() const { return m_Y; }
    void GetPosition(float& x, float& y) const { x = m_X; y = m_Y; }

    EVENT_CLASS_METHODS(MouseMoved)

private:
    float m_X, m_Y;
};
}
