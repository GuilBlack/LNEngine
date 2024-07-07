#pragma once

namespace lne
{
using KeyCode = uint16_t;
using KeyState = uint8_t;
using MouseButtonState = uint8_t;
using MouseButton = uint16_t;

#define MAX_KEYS 512
#define MAX_MOUSE_BUTTONS 8

enum : KeyState
{
    eKeyUp =  0b00000000,
    eKeyDown =   0b00000001,
    eKeyRepeated =  0b00000011
};

enum : MouseButtonState
{
    eMouseButtonUp = 0b00000000,
    eMouseButtonDown =  0b00000001
};

enum : KeyCode
{
    //signs
    eKeySpace =          32,
    eKeyApostrophe =     39, /* ' */
    eKeyComma =          44, /* , */
    eKeyMinus =          45, /* - */
    eKeyPeriod =         46, /* . */
    eKeySlash =          47, /* / */
    eKeySemicolon =      59, /* ; */
    eKeyEqual =          61, /* = */
    eKeyLeftBracket =    91, /* [ */
    eKeyBackslash =      92, /* \ */
    eKeyRightBracket =   93, /* ] */
    eKeyGraveAccent =    96, /* ` */

    // Numbers
    eKey0 = 48,
    eKey1 = 49,
    eKey2 = 50,
    eKey3 = 51,
    eKey4 = 52,
    eKey5 = 53,
    eKey6 = 54,
    eKey7 = 55,
    eKey8 = 56,
    eKey9 = 57,

    //Alphabetic character
    eKeyA = 65,
    eKeyB = 66,
    eKeyC = 67,
    eKeyD = 68,
    eKeyE = 69,
    eKeyF = 70,
    eKeyG = 71,
    eKeyH = 72,
    eKeyI = 73,
    eKeyJ = 74,
    eKeyK = 75,
    eKeyL = 76,
    eKeyM = 77,
    eKeyN = 78,
    eKeyO = 79,
    eKeyP = 80,
    eKeyQ = 81,
    eKeyR = 82,
    eKeyS = 83,
    eKeyT = 84,
    eKeyU = 85,
    eKeyV = 86,
    eKeyW = 87,
    eKeyX = 88,
    eKeyY = 89,
    eKeyZ = 90,

    eKeyWorld1 = 161, /* non-US #1 */
    eKeyWorld2 = 162, /* non-US #2 */

    // Function keys
    eKeyEscape =         256,
    eKeyEnter =          257,
    eKeyTab =            258,
    eKeyBackspace =      259,
    eKeyInsert =         260,
    eKeyDelete =         261,
    eKeyArrowRight =     262,
    eKeyArrowLeft =      263,
    eKeyArrowDown =      264,
    eKeyArrowUp =        265,
    eKeyPageUp =         266,
    eKeyPageDown =       267,
    eKeyHome =           268,
    eKeyEnd =            269,
    eKeyCapsLock =       280,
    eKeyScrollLock =     281,
    eKeyNumLock =        282,
    eKeyPrintScreen =    283,
    eKeyPause =          284,
    eKeyF1 =     290,
    eKeyF2 =     291,
    eKeyF3 =     292,
    eKeyF4 =     293,
    eKeyF5 =     294,
    eKeyF6 =     295,
    eKeyF7 =     296,
    eKeyF8 =     297,
    eKeyF9 =     298,
    eKeyF10 =    299,
    eKeyF11 =    300,
    eKeyF12 =    301,
    eKeyF13 =    302,
    eKeyF14 =    303,
    eKeyF15 =    304,
    eKeyF16 =    305,
    eKeyF17 =    306,
    eKeyF18 =    307,
    eKeyF19 =    308,
    eKeyF20 =    309,
    eKeyF21 =    310,
    eKeyF22 =    311,
    eKeyF23 =    312,
    eKeyF24 =    313,
    eKeyF25 =    314,

    // Keypad
    eKeyP0 = 320,
    eKeyP1 = 321,
    eKeyP2 = 322,
    eKeyP3 = 323,
    eKeyP4 = 324,
    eKeyP5 = 325,
    eKeyP6 = 326,
    eKeyP7 = 327,
    eKeyP8 = 328,
    eKeyP9 = 329,
    eKeyPDecimal =   330,
    eKeyPDivide =    331,
    eKeyPMultiply =  332,
    eKeyPSubtract =  333,
    eKeyPAdd =       334,
    eKeyPEnter =     335,
    eKeyPEqual =     336,

    // Modifiers
    eKeyLeftShift =      340,
    eKeyLeftControl =    341,
    eKeyLeftAlt =        342,
    eKeyLeftSuper =      343,
    eKeyRightShift =     344,
    eKeyRightControl =   345,
    eKeyRightAlt =       346,
    eKeyRightSuper =     347,
    eKeyMenu =           348,
    eKeyLast =           eKeyMenu,
    eKeyFirst =          eKeySpace
};

std::string_view KeyCodeToString(KeyCode keyCode);

enum : MouseButton
{
    eMouseButton0 =        0,
    eMouseButton1 =        1,
    eMouseButton2 =        2,
    eMouseButton3 =        3,
    eMouseButton4 =        4,
    eMouseButton5 =        5,
    eMouseButton6 =        6,
    eMouseButton7 =        7,

    eMouseLeft =    eMouseButton0,
    eMouseRight =   eMouseButton1,
    eMouseMiddle =  eMouseButton2,

    eMouseButtonFirst =  eMouseButton0,
    eMouseButtonLast =   eMouseButton7
};

std::string_view MouseButtonToString(MouseButton mouseButton);

}
