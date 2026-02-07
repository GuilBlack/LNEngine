#pragma once
#include "Events.h"

namespace lne
{
class WindowCloseEvent : public Event
{
public:
    WindowCloseEvent() = default;

    EVENT_CLASS_METHODS(WindowClose)
};

class WindowResizeEvent : public Event
{
public:
    WindowResizeEvent(u32 width, u32 height)
        : m_Width(width), m_Height(height) {}

    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

    EVENT_CLASS_METHODS(WindowResize)

private:
    u32 m_Width;
    u32 m_Height;
};
}
