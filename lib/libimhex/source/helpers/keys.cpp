#include <hex/helpers/keys.hpp>

// These values are the stable GLFW key codes used by shortcut settings written before schema v2.
enum Keys scanCodeToKey(int scanCode) {
    if (scanCode >= 48 && scanCode <= 57)
        return Keys(int(Keys::Num0) + scanCode - 48);
    if (scanCode >= 65 && scanCode <= 90)
        return Keys(int(Keys::A) + scanCode - 65);
    if (scanCode >= 290 && scanCode <= 314)
        return Keys(int(Keys::F1) + scanCode - 290);
    if (scanCode >= 320 && scanCode <= 329)
        return Keys(int(Keys::KeyPad0) + scanCode - 320);

    switch (scanCode) {
        case 32:  return Keys::Space;
        case 39:  return Keys::Apostrophe;
        case 44:  return Keys::Comma;
        case 45:  return Keys::Minus;
        case 46:  return Keys::Period;
        case 47:  return Keys::Slash;
        case 59:  return Keys::Semicolon;
        case 61:  return Keys::Equals;
        case 91:  return Keys::LeftBracket;
        case 92:  return Keys::Backslash;
        case 93:  return Keys::RightBracket;
        case 96:  return Keys::GraveAccent;
        case 161: return Keys::World1;
        case 162: return Keys::World2;
        case 256: return Keys::Escape;
        case 257: return Keys::Enter;
        case 258: return Keys::Tab;
        case 259: return Keys::Backspace;
        case 260: return Keys::Insert;
        case 261: return Keys::Delete;
        case 262: return Keys::Right;
        case 263: return Keys::Left;
        case 264: return Keys::Down;
        case 265: return Keys::Up;
        case 266: return Keys::PageUp;
        case 267: return Keys::PageDown;
        case 268: return Keys::Home;
        case 269: return Keys::End;
        case 280: return Keys::CapsLock;
        case 281: return Keys::ScrollLock;
        case 282: return Keys::NumLock;
        case 283: return Keys::PrintScreen;
        case 284: return Keys::Pause;
        case 330: return Keys::KeyPadDecimal;
        case 331: return Keys::KeyPadDivide;
        case 332: return Keys::KeyPadMultiply;
        case 333: return Keys::KeyPadSubtract;
        case 334: return Keys::KeyPadAdd;
        case 335: return Keys::KeyPadEnter;
        case 336: return Keys::KeyPadEqual;
        case 348: return Keys::Menu;
        default:  return Keys::Invalid;
    }
}

int keyToScanCode(enum Keys key) {
    const auto value = int(key);
    if (key >= Keys::Num0 && key <= Keys::Num9)
        return 48 + value - int(Keys::Num0);
    if (key >= Keys::A && key <= Keys::Z)
        return 65 + value - int(Keys::A);
    if (key >= Keys::F1 && key <= Keys::F25)
        return 290 + value - int(Keys::F1);
    if (key >= Keys::KeyPad0 && key <= Keys::KeyPad9)
        return 320 + value - int(Keys::KeyPad0);

    switch (key) {
        case Keys::Space:          return 32;
        case Keys::Apostrophe:     return 39;
        case Keys::Comma:          return 44;
        case Keys::Minus:          return 45;
        case Keys::Period:         return 46;
        case Keys::Slash:          return 47;
        case Keys::Semicolon:      return 59;
        case Keys::Equals:         return 61;
        case Keys::LeftBracket:    return 91;
        case Keys::Backslash:      return 92;
        case Keys::RightBracket:   return 93;
        case Keys::GraveAccent:    return 96;
        case Keys::World1:         return 161;
        case Keys::World2:         return 162;
        case Keys::Escape:         return 256;
        case Keys::Enter:          return 257;
        case Keys::Tab:            return 258;
        case Keys::Backspace:      return 259;
        case Keys::Insert:         return 260;
        case Keys::Delete:         return 261;
        case Keys::Right:          return 262;
        case Keys::Left:           return 263;
        case Keys::Down:           return 264;
        case Keys::Up:             return 265;
        case Keys::PageUp:         return 266;
        case Keys::PageDown:       return 267;
        case Keys::Home:           return 268;
        case Keys::End:            return 269;
        case Keys::CapsLock:       return 280;
        case Keys::ScrollLock:     return 281;
        case Keys::NumLock:        return 282;
        case Keys::PrintScreen:    return 283;
        case Keys::Pause:          return 284;
        case Keys::KeyPadDecimal:  return 330;
        case Keys::KeyPadDivide:   return 331;
        case Keys::KeyPadMultiply: return 332;
        case Keys::KeyPadSubtract: return 333;
        case Keys::KeyPadAdd:      return 334;
        case Keys::KeyPadEnter:    return 335;
        case Keys::KeyPadEqual:    return 336;
        case Keys::Menu:           return 348;
        default:                   return 0;
    }
}
