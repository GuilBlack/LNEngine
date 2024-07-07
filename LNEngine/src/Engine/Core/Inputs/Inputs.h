#pragma once
#include "InputCodes.h"
#include "Engine/Core/Events/KeyboardEvents.h"
#include "Engine/Core/Events/MouseEvents.h"
#include "Engine/Core/Events/ApplicationEvents.h"
#include "Engine/Core/Events/MouseEvents.h"
#include "Engine/Core/Utils/Defines.h"

namespace lne
{
class InputManager
{
    struct KeyboardState
    {
        uint8_t Keys[MAX_KEYS]{};
    };

    struct MouseState
    {
        bool Buttons[MAX_MOUSE_BUTTONS]{};
        float X{}, Y{};
    };

    struct InputStates
    {
        KeyboardState Keyboard{};
        MouseState Mouse{};
        KeyboardState OldKeyboard{};
        MouseState OldMouse{};
    };

public:
    InputManager();
    ~InputManager();

    bool UpdateStates(AppUpdatedEvent& e);

    bool IsKeyPressed(KeyCode keycode);
    bool IsKeyReleased(KeyCode keycode);
    bool IsMouseButtonPressed(MouseButton button);
    bool IsMouseButtonReleased(MouseButton button);
    void GetMousePosition(float& x, float& y);

private:
    InputStates m_States;

private:
    bool ProcessKeyPressedEvent(KeyPressedEvent& e);
    bool ProcessKeyReleasedEvent(KeyReleasedEvent& e);
    bool ProcessMousePressedEvent(MouseButtonPressedEvent& e);
    bool ProcessMouseReleasedEvent(MouseButtonReleasedEvent& e);
    bool ProcessMouseMovedEvent(MouseMovedEvent& e);
};
}
