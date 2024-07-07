#pragma once
#include "Events.h"
#include "../Inputs/InputCodes.h"

namespace lne
{
class KeyPressedEvent : public Event
{
public:
    KeyPressedEvent(KeyCode keycode, bool repeat = false)
        : m_KeyCode{ keycode }, m_Repeat{ repeat }
    {}

    KeyCode GetKeyCode() const { return m_KeyCode; }
    bool IsRepeat() const { return m_Repeat; }

    EVENT_CLASS_METHODS(KeyPressed)

private:
    KeyCode m_KeyCode;
    bool m_Repeat;
};

class KeyReleasedEvent : public Event
{
public:
    KeyReleasedEvent(KeyCode keycode)
        : m_KeyCode(keycode)
    {}

    KeyCode GetKeyCode() const { return m_KeyCode; }

    EVENT_CLASS_METHODS(KeyReleased)
private:
    KeyCode m_KeyCode;
};
}
