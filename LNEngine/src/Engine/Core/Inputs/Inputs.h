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

    [[nodiscard]] bool UpdateStates(AppUpdatedEvent& e);

    [[nodiscard]] bool IsKeyPressed(KeyCode keycode);
    [[nodiscard]] bool IsKeyReleased(KeyCode keycode);
    [[nodiscard]] bool IsMouseButtonPressed(MouseButton button);
    [[nodiscard]] bool IsMouseButtonReleased(MouseButton button);
    void GetMousePosition(float& x, float& y);

private:
    InputStates m_States;

private:
    [[nodiscard]] bool ProcessKeyPressedEvent(KeyPressedEvent& e);
    [[nodiscard]] bool ProcessKeyReleasedEvent(KeyReleasedEvent& e);
    [[nodiscard]] bool ProcessMousePressedEvent(MouseButtonPressedEvent& e);
    [[nodiscard]] bool ProcessMouseReleasedEvent(MouseButtonReleasedEvent& e);
    [[nodiscard]] bool ProcessMouseMovedEvent(MouseMovedEvent& e);
};
}
