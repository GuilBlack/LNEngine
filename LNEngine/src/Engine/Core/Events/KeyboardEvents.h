#pragma once
#include "Events.h"
#include "../Inputs/InputCodes.h"

namespace lne
{
class KeyPressedEvent : public Event
{
public:
    KeyPressedEvent(KeyCode keycode)
        : m_KeyCode(keycode) {}

    KeyCode GetKeyCode() const { return m_KeyCode; }

    EVENT_CLASS_METHODS(KeyPressed)

private:
    KeyCode m_KeyCode;
    bool m_Repeat;
};

class KeyReleased : public Event
{
public:
    KeyReleased(KeyCode keycode)
        : m_KeyCode(keycode)
    {}

    KeyCode GetKeyCode() const { return m_KeyCode; }

    EVENT_CLASS_METHODS(KeyReleased)
private:
    KeyCode m_KeyCode;
};
}
}
