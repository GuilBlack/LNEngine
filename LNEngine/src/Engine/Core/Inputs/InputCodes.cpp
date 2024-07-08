#include "lnepch.h"
#include "InputCodes.h"

namespace lne
{
std::string_view lne::KeyCodeToString(KeyCode keyCode)
{
    // FIXME: this order isn't quite right
    static const std::string_view keyStrings[MAX_KEYS] = {
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 0-15
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 16-31
           "Space", "", "", "", "", "", "", "Apostrophe", "", "", "", "", "Comma", "Minus", "Period", "Slash", // 32-47
           "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "", "Semicolon", "", "Equal", "", "", // 48-63
           "", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", // 64-79
           "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "LeftBracket", "Backslash", "RightBracket", "", "", // 80-95
           "GraveAccent", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 96-111
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 112-127
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 128-143
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 144-159
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 160-175
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 176-191
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 192-207
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 208-223
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 224-239
           "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", // 240-255
           "Escape", "Enter", "Tab", "Backspace", "Insert", "Delete", "ArrowRight", "ArrowLeft", "ArrowDown", "ArrowUp", "PageUp", "PageDown", "Home", "End", // 256-269
           "", "", "", "", "", "", "", "", "", "", "CapsLock", "ScrollLock", "NumLock", "PrintScreen", "Pause", "", // 270-285
           "", "", "", "", "", // 286-290
           "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16", // 291-306
           "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "F25", // 307-314
           "", "", "", "", "", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "P9", "PDecimal", // 315-330
           "PDivide", "PMultiply", "PSubtract", "PAdd", "PEnter", "PEqual", "", "", "", // 331-339
           "LeftShift", "LeftControl", "LeftAlt", "LeftSuper", "RightShift", "RightControl", "RightAlt",
           "RightSuper", "Menu" // 340-348
    };

    if (keyCode <= eKeyLast && keyCode >= eKeyFirst)
        return keyStrings[keyCode];

    return "";

}
std::string_view MouseButtonToString(MouseButton mouseButton)
{
    static const std::string_view mouseButtonStrings[MAX_MOUSE_BUTTONS] = {
        "MouseButtonLeft", "MouseButtonRight", "MouseButtonMiddle", "Mouse3", "Mouse4", "Mouse5", "Mouse6", "Mouse7"
    };

    if (mouseButton < MAX_MOUSE_BUTTONS && mouseButton >= 0)
        return mouseButtonStrings[mouseButton];

    return "";
}
}
