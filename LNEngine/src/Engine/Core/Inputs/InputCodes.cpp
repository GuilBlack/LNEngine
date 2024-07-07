#include "lnepch.h"
#include "InputCodes.h"

namespace lne
{
KeyCode MapGLFWKeyToKeyCode(int glfwKey)
{
    switch (glfwKey)
    {
    case GLFW_KEY_SPACE: return KeySpace;
    case GLFW_KEY_APOSTROPHE: return KeyQuote; // '
    case GLFW_KEY_COMMA: return KeyComma; // ,
    case GLFW_KEY_MINUS: return KeyMinus; // -
    case GLFW_KEY_PERIOD: return KeyPeriod; // .
    case GLFW_KEY_SLASH: return KeySlash; // /
    case GLFW_KEY_0: return Key0;
    case GLFW_KEY_1: return Key1;
    case GLFW_KEY_2: return Key2;
    case GLFW_KEY_3: return Key3;
    case GLFW_KEY_4: return Key4;
    case GLFW_KEY_5: return Key5;
    case GLFW_KEY_6: return Key6;
    case GLFW_KEY_7: return Key7;
    case GLFW_KEY_8: return Key8;
    case GLFW_KEY_9: return Key9;
    case GLFW_KEY_SEMICOLON: return KeySemicolon; // ;
    case GLFW_KEY_EQUAL: return KeyEqual; // =
    case GLFW_KEY_A: return KeyA;
    case GLFW_KEY_B: return KeyB;
    case GLFW_KEY_C: return KeyC;
    case GLFW_KEY_D: return KeyD;
    case GLFW_KEY_E: return KeyE;
    case GLFW_KEY_F: return KeyF;
    case GLFW_KEY_G: return KeyG;
    case GLFW_KEY_H: return KeyH;
    case GLFW_KEY_I: return KeyI;
    case GLFW_KEY_J: return KeyJ;
    case GLFW_KEY_K: return KeyK;
    case GLFW_KEY_L: return KeyL;
    case GLFW_KEY_M: return KeyM;
    case GLFW_KEY_N: return KeyN;
    case GLFW_KEY_O: return KeyO;
    case GLFW_KEY_P: return KeyP;
    case GLFW_KEY_Q: return KeyQ;
    case GLFW_KEY_R: return KeyR;
    case GLFW_KEY_S: return KeyS;
    case GLFW_KEY_T: return KeyT;
    case GLFW_KEY_U: return KeyU;
    case GLFW_KEY_V: return KeyV;
    case GLFW_KEY_W: return KeyW;
    case GLFW_KEY_X: return KeyX;
    case GLFW_KEY_Y: return KeyY;
    case GLFW_KEY_Z: return KeyZ;
    case GLFW_KEY_LEFT_BRACKET: return KeyLeftBracket; // [
    case GLFW_KEY_BACKSLASH: return KeyBackslash; // \ 
    case GLFW_KEY_RIGHT_BRACKET: return KeyRightBracket; // ]
    case GLFW_KEY_GRAVE_ACCENT: return KeyGrave; // `
    case GLFW_KEY_WORLD_1: return KeyOEM102; // Non-US #
    case GLFW_KEY_ESCAPE: return KeyEscape;
    case GLFW_KEY_ENTER: return KeyReturn;
    case GLFW_KEY_TAB: return KeyTab;
    case GLFW_KEY_BACKSPACE: return KeyBackspace;
    case GLFW_KEY_INSERT: return KeyInsert;
    case GLFW_KEY_DELETE: return KeyDelete;
    case GLFW_KEY_RIGHT: return KeyRight;
    case GLFW_KEY_LEFT: return KeyLeft;
    case GLFW_KEY_DOWN: return KeyDown;
    case GLFW_KEY_UP: return KeyUp;
    case GLFW_KEY_PAGE_UP: return KeyPrior;
    case GLFW_KEY_PAGE_DOWN: return KeyNext;
    case GLFW_KEY_HOME: return KeyHome;
    case GLFW_KEY_END: return KeyEnd;
    case GLFW_KEY_CAPS_LOCK: return KeyCapital;
    case GLFW_KEY_SCROLL_LOCK: return KeyScroll;
    case GLFW_KEY_NUM_LOCK: return KeyNumLock;
    case GLFW_KEY_PRINT_SCREEN: return KeySnapshot;
    case GLFW_KEY_PAUSE: return KeyPause;
    case GLFW_KEY_F1: return KeyF1;
    case GLFW_KEY_F2: return KeyF2;
    case GLFW_KEY_F3: return KeyF3;
    case GLFW_KEY_F4: return KeyF4;
    case GLFW_KEY_F5: return KeyF5;
    case GLFW_KEY_F6: return KeyF6;
    case GLFW_KEY_F7: return KeyF7;
    case GLFW_KEY_F8: return KeyF8;
    case GLFW_KEY_F9: return KeyF9;
    case GLFW_KEY_F10: return KeyF10;
    case GLFW_KEY_F11: return KeyF11;
    case GLFW_KEY_F12: return KeyF12;
    case GLFW_KEY_F13: return KeyF13;
    case GLFW_KEY_F14: return KeyF14;
    case GLFW_KEY_F15: return KeyF15;
    case GLFW_KEY_F16: return KeyF16;
    case GLFW_KEY_F17: return KeyF17;
    case GLFW_KEY_F18: return KeyF18;
    case GLFW_KEY_F19: return KeyF19;
    case GLFW_KEY_F20: return KeyF20;
    case GLFW_KEY_F21: return KeyF21;
    case GLFW_KEY_F22: return KeyF22;
    case GLFW_KEY_F23: return KeyF23;
    case GLFW_KEY_F24: return KeyF24;
    case GLFW_KEY_KP_0: return KeyNumpad0;
    case GLFW_KEY_KP_1: return KeyNumpad1;
    case GLFW_KEY_KP_2: return KeyNumpad2;
    case GLFW_KEY_KP_3: return KeyNumpad3;
    case GLFW_KEY_KP_4: return KeyNumpad4;
    case GLFW_KEY_KP_5: return KeyNumpad5;
    case GLFW_KEY_KP_6: return KeyNumpad6;
    case GLFW_KEY_KP_7: return KeyNumpad7;
    case GLFW_KEY_KP_8: return KeyNumpad8;
    case GLFW_KEY_KP_9: return KeyNumpad9;
    case GLFW_KEY_KP_DECIMAL: return KeyDecimal;
    case GLFW_KEY_KP_DIVIDE: return KeyDivide;
    case GLFW_KEY_KP_MULTIPLY: return KeyMultiply;
    case GLFW_KEY_KP_SUBTRACT: return KeySubtract;
    case GLFW_KEY_KP_ADD: return KeyAdd;
    case GLFW_KEY_KP_ENTER: return KeyReturn;
    case GLFW_KEY_KP_EQUAL: return KeyEqual;
    case GLFW_KEY_LEFT_SHIFT: return KeyLShift;
    case GLFW_KEY_LEFT_CONTROL: return KeyLControl;
    case GLFW_KEY_LEFT_ALT: return KeyLMenu;
    case GLFW_KEY_LEFT_SUPER: return KeyLWin;
    case GLFW_KEY_RIGHT_SHIFT: return KeyRShift;
    case GLFW_KEY_RIGHT_CONTROL: return KeyRControl;
    case GLFW_KEY_RIGHT_ALT: return KeyRMenu;
    case GLFW_KEY_RIGHT_SUPER: return KeyRWin;
    case GLFW_KEY_MENU: return KeyMenu;
    default: return 0; // Or some invalid KeyCode value
    }
}

int MapKeyCodeToGLFWKey(KeyCode keyCode)
{
    switch (keyCode)
    {
    case KeySpace: return GLFW_KEY_SPACE;
    case KeyQuote: return GLFW_KEY_APOSTROPHE; // '
    case KeyComma: return GLFW_KEY_COMMA; // ,
    case KeyMinus: return GLFW_KEY_MINUS; // -
    case KeyPeriod: return GLFW_KEY_PERIOD; // .
    case KeySlash: return GLFW_KEY_SLASH; // /
    case Key0: return GLFW_KEY_0;
    case Key1: return GLFW_KEY_1;
    case Key2: return GLFW_KEY_2;
    case Key3: return GLFW_KEY_3;
    case Key4: return GLFW_KEY_4;
    case Key5: return GLFW_KEY_5;
    case Key6: return GLFW_KEY_6;
    case Key7: return GLFW_KEY_7;
    case Key8: return GLFW_KEY_8;
    case Key9: return GLFW_KEY_9;
    case KeySemicolon: return GLFW_KEY_SEMICOLON; // ;
    case KeyEqual: return GLFW_KEY_EQUAL; // =
    case KeyA: return GLFW_KEY_A;
    case KeyB: return GLFW_KEY_B;
    case KeyC: return GLFW_KEY_C;
    case KeyD: return GLFW_KEY_D;
    case KeyE: return GLFW_KEY_E;
    case KeyF: return GLFW_KEY_F;
    case KeyG: return GLFW_KEY_G;
    case KeyH: return GLFW_KEY_H;
    case KeyI: return GLFW_KEY_I;
    case KeyJ: return GLFW_KEY_J;
    case KeyK: return GLFW_KEY_K;
    case KeyL: return GLFW_KEY_L;
    case KeyM: return GLFW_KEY_M;
    case KeyN: return GLFW_KEY_N;
    case KeyO: return GLFW_KEY_O;
    case KeyP: return GLFW_KEY_P;
    case KeyQ: return GLFW_KEY_Q;
    case KeyR: return GLFW_KEY_R;
    case KeyS: return GLFW_KEY_S;
    case KeyT: return GLFW_KEY_T;
    case KeyU: return GLFW_KEY_U;
    case KeyV: return GLFW_KEY_V;
    case KeyW: return GLFW_KEY_W;
    case KeyX: return GLFW_KEY_X;
    case KeyY: return GLFW_KEY_Y;
    case KeyZ: return GLFW_KEY_Z;
    case KeyLeftBracket: return GLFW_KEY_LEFT_BRACKET; // [
    case KeyBackslash: return GLFW_KEY_BACKSLASH; // \
                        case KeyRightBracket: return GLFW_KEY_RIGHT_BRACKET; // ]
    case KeyGrave: return GLFW_KEY_GRAVE_ACCENT; // `
    case KeyOEM102: return GLFW_KEY_WORLD_1; // Non-US #
    case KeyEscape: return GLFW_KEY_ESCAPE;
    case KeyReturn: return GLFW_KEY_ENTER;
    case KeyTab: return GLFW_KEY_TAB;
    case KeyBackspace: return GLFW_KEY_BACKSPACE;
    case KeyInsert: return GLFW_KEY_INSERT;
    case KeyDelete: return GLFW_KEY_DELETE;
    case KeyRight: return GLFW_KEY_RIGHT;
    case KeyLeft: return GLFW_KEY_LEFT;
    case KeyDown: return GLFW_KEY_DOWN;
    case KeyUp: return GLFW_KEY_UP;
    case KeyPrior: return GLFW_KEY_PAGE_UP;
    case KeyNext: return GLFW_KEY_PAGE_DOWN;
    case KeyHome: return GLFW_KEY_HOME;
    case KeyEnd: return GLFW_KEY_END;
    case KeyCapital: return GLFW_KEY_CAPS_LOCK;
    case KeyScroll: return GLFW_KEY_SCROLL_LOCK;
    case KeyNumLock: return GLFW_KEY_NUM_LOCK;
    case KeySnapshot: return GLFW_KEY_PRINT_SCREEN;
    case KeyPause: return GLFW_KEY_PAUSE;
    case KeyF1: return GLFW_KEY_F1;
    case KeyF2: return GLFW_KEY_F2;
    case KeyF3: return GLFW_KEY_F3;
    case KeyF4: return GLFW_KEY_F4;
    case KeyF5: return GLFW_KEY_F5;
    case KeyF6: return GLFW_KEY_F6;
    case KeyF7: return GLFW_KEY_F7;
    case KeyF8: return GLFW_KEY_F8;
    case KeyF9: return GLFW_KEY_F9;
    case KeyF10: return GLFW_KEY_F10;
    case KeyF11: return GLFW_KEY_F11;
    case KeyF12: return GLFW_KEY_F12;
    case KeyF13: return GLFW_KEY_F13;
    case KeyF14: return GLFW_KEY_F14;
    case KeyF15: return GLFW_KEY_F15;
    case KeyF16: return GLFW_KEY_F16;
    case KeyF17: return GLFW_KEY_F17;
    case KeyF18: return GLFW_KEY_F18;
    case KeyF19: return GLFW_KEY_F19;
    case KeyF20: return GLFW_KEY_F20;
    case KeyF21: return GLFW_KEY_F21;
    case KeyF22: return GLFW_KEY_F22;
    case KeyF23: return GLFW_KEY_F23;
    case KeyF24: return GLFW_KEY_F24;
    case KeyNumpad0: return GLFW_KEY_KP_0;
    case KeyNumpad1: return GLFW_KEY_KP_1;
    case KeyNumpad2: return GLFW_KEY_KP_2;
    case KeyNumpad3: return GLFW_KEY_KP_3;
    case KeyNumpad4: return GLFW_KEY_KP_4;
    case KeyNumpad5: return GLFW_KEY_KP_5;
    case KeyNumpad6: return GLFW_KEY_KP_6;
    case KeyNumpad7: return GLFW_KEY_KP_7;
    case KeyNumpad8: return GLFW_KEY_KP_8;
    case KeyNumpad9: return GLFW_KEY_KP_9;
    case KeyDecimal: return GLFW_KEY_KP_DECIMAL;
    case KeyDivide: return GLFW_KEY_KP_DIVIDE;
    case KeyMultiply: return GLFW_KEY_KP_MULTIPLY;
    case KeySubtract: return GLFW_KEY_KP_SUBTRACT;
    case KeyAdd: return GLFW_KEY_KP_ADD;
    case KeyLShift: return GLFW_KEY_LEFT_SHIFT;
    case KeyLControl: return GLFW_KEY_LEFT_CONTROL;
    case KeyLMenu: return GLFW_KEY_LEFT_ALT;
    case KeyLWin: return GLFW_KEY_LEFT_SUPER;
    case KeyRShift: return GLFW_KEY_RIGHT_SHIFT;
    case KeyRControl: return GLFW_KEY_RIGHT_CONTROL;
    case KeyRMenu: return GLFW_KEY_RIGHT_ALT;
    case KeyRWin: return GLFW_KEY_RIGHT_SUPER;
    case KeyMenu: return GLFW_KEY_MENU;
    default: return GLFW_KEY_UNKNOWN; // Or any other GLFW unknown key code
    }
}
}
