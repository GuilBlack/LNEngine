#include "Inputs.h"
#include "Core/ApplicationBase.h"
#include "Core/Utils/Log.h"
#include "Graphics/GfxContext.h"
#include "Graphics/Swapchain.h"
#include "Graphics/Texture.h"
#include "Resources/GfxLoader.h"

namespace lne
{
// TODO: handle ImGui input captures
InputManager::InputManager()
{
    ApplicationBase::GetEventHub().RegisterListener<KeyPressedEvent>(this, &InputManager::ProcessKeyPressedEvent);
    ApplicationBase::GetEventHub().RegisterListener<KeyReleasedEvent>(this, &InputManager::ProcessKeyReleasedEvent);
    ApplicationBase::GetEventHub().RegisterListener<AppUpdatedEvent>(this, &InputManager::UpdateStates);
    ApplicationBase::GetEventHub().RegisterListener<MouseButtonPressedEvent>(this, &InputManager::ProcessMousePressedEvent);
    ApplicationBase::GetEventHub().RegisterListener<MouseButtonReleasedEvent>(this, &InputManager::ProcessMouseReleasedEvent);
    ApplicationBase::GetEventHub().RegisterListener<MouseMovedEvent>(this, &InputManager::ProcessMouseMovedEvent);
}

InputManager::~InputManager()
{
    ApplicationBase::GetEventHub().UnregisterListener<KeyPressedEvent>(this);
    ApplicationBase::GetEventHub().UnregisterListener<KeyReleasedEvent>(this);
    ApplicationBase::GetEventHub().UnregisterListener<AppUpdatedEvent>(this);
}

bool InputManager::UpdateStates(AppUpdatedEvent& e)
{
    memcpy(&m_States.OldKeyboard, &m_States.Keyboard, sizeof(KeyboardState));
    memcpy(&m_States.OldMouse, &m_States.Mouse, sizeof(MouseState));
    return false;
}

bool InputManager::IsKeyPressed(KeyCode keycode)
{
    if (keycode < eKeyFirst || keycode > eKeyLast)
        return false;

    return m_States.Keyboard.Keys[keycode] & eKeyDown;
}

bool InputManager::IsKeyReleased(KeyCode keycode)
{
    if (keycode < eKeyFirst || keycode > eKeyLast)
        return false;

    return m_States.Keyboard.Keys[keycode] == eKeyUp;
}

bool InputManager::IsMouseButtonPressed(MouseButton button)
{
    if (button < eMouseButtonFirst || button > eMouseButtonLast)
        return false;
    return m_States.Mouse.Buttons[button] == eMouseButtonDown;
}

bool InputManager::IsMouseButtonReleased(MouseButton button)
{
    if (button < eMouseButtonFirst || button > eMouseButtonLast)
        return false;
    return m_States.Mouse.Buttons[button] == eMouseButtonUp;
}

void InputManager::GetMousePosition(float& x, float& y)
{
    x = m_States.Mouse.X;
    y = m_States.Mouse.Y;
}

void InputManager::GetMouseDelta(float& x, float& y)
{
    x = m_States.Mouse.X - m_States.OldMouse.X;
    y = m_States.Mouse.Y - m_States.OldMouse.Y;
}

bool InputManager::ProcessKeyPressedEvent(KeyPressedEvent& e)
{
    m_States.Keyboard.Keys[e.GetKeyCode()] = e.IsRepeat() ? eKeyRepeated : eKeyDown;
    return false;
}

bool InputManager::ProcessKeyReleasedEvent(KeyReleasedEvent& e)
{
    m_States.Keyboard.Keys[e.GetKeyCode()] = eKeyUp;
    return false;
}

bool InputManager::ProcessMousePressedEvent(MouseButtonPressedEvent& e)
{
    m_States.Mouse.Buttons[e.GetButton()] = eMouseButtonDown;
    return false;
}
bool InputManager::ProcessMouseReleasedEvent(MouseButtonReleasedEvent& e)
{
    m_States.Mouse.Buttons[e.GetButton()] = eMouseButtonUp;
    return false;
}
bool InputManager::ProcessMouseMovedEvent(MouseMovedEvent& e)
{
    e.GetPosition(m_States.Mouse.X, m_States.Mouse.Y);
    return false;
}
}
