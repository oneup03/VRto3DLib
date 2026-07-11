/*
 * This file is part of VRto3D.
 *
 * VRto3D is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VRto3D is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with VRto3D. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

// Portable key currency: Windows VK_* codes and XINPUT_GAMEPAD_* button
// bitmasks. These plain ints are what configs/profiles store and what the
// input backends speak. On Windows the real headers provide them; elsewhere
// the same numeric values are defined here so shared driver code compiles
// unchanged on both platforms.

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <XInput.h>

#else  // !_WIN32

// Canonical Win32 virtual-key values (winuser.h).
#define VK_LBUTTON    0x01
#define VK_RBUTTON    0x02
#define VK_MBUTTON    0x04
#define VK_XBUTTON1   0x05
#define VK_XBUTTON2   0x06
#define VK_BACK       0x08
#define VK_TAB        0x09
#define VK_RETURN     0x0D
#define VK_SHIFT      0x10
#define VK_CONTROL    0x11
#define VK_MENU       0x12
#define VK_PAUSE      0x13
#define VK_CAPITAL    0x14
#define VK_ESCAPE     0x1B
#define VK_SPACE      0x20
#define VK_PRIOR      0x21
#define VK_NEXT       0x22
#define VK_END        0x23
#define VK_HOME       0x24
#define VK_LEFT       0x25
#define VK_UP         0x26
#define VK_RIGHT      0x27
#define VK_DOWN       0x28
#define VK_SNAPSHOT   0x2C
#define VK_INSERT     0x2D
#define VK_DELETE     0x2E
#define VK_LWIN       0x5B
#define VK_RWIN       0x5C
#define VK_NUMPAD0    0x60
#define VK_NUMPAD1    0x61
#define VK_NUMPAD2    0x62
#define VK_NUMPAD3    0x63
#define VK_NUMPAD4    0x64
#define VK_NUMPAD5    0x65
#define VK_NUMPAD6    0x66
#define VK_NUMPAD7    0x67
#define VK_NUMPAD8    0x68
#define VK_NUMPAD9    0x69
#define VK_MULTIPLY   0x6A
#define VK_ADD        0x6B
#define VK_SUBTRACT   0x6D
#define VK_DECIMAL    0x6E
#define VK_DIVIDE     0x6F
#define VK_F1         0x70
#define VK_F2         0x71
#define VK_F3         0x72
#define VK_F4         0x73
#define VK_F5         0x74
#define VK_F6         0x75
#define VK_F7         0x76
#define VK_F8         0x77
#define VK_F9         0x78
#define VK_F10        0x79
#define VK_F11        0x7A
#define VK_F12        0x7B
#define VK_F13        0x7C
#define VK_F14        0x7D
#define VK_F15        0x7E
#define VK_F16        0x7F
#define VK_F17        0x80
#define VK_F18        0x81
#define VK_F19        0x82
#define VK_F20        0x83
#define VK_F21        0x84
#define VK_F22        0x85
#define VK_F23        0x86
#define VK_F24        0x87
#define VK_LSHIFT     0xA0
#define VK_RSHIFT     0xA1
#define VK_LCONTROL   0xA2
#define VK_RCONTROL   0xA3
#define VK_LMENU      0xA4
#define VK_RMENU      0xA5
#define VK_OEM_1      0xBA  // ;:
#define VK_OEM_PLUS   0xBB  // =+
#define VK_OEM_COMMA  0xBC
#define VK_OEM_MINUS  0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2      0xBF  // /?
#define VK_OEM_3      0xC0  // `~
#define VK_OEM_4      0xDB  // [{
#define VK_OEM_5      0xDC  // \|
#define VK_OEM_6      0xDD  // ]}
#define VK_OEM_7      0xDE  // '"

// XINPUT_GAMEPAD wButtons bits (XInput.h).
#define XINPUT_GAMEPAD_DPAD_UP        0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN      0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT      0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT     0x0008
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB     0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB    0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000

#endif  // _WIN32

// VRto3D extensions: guide button (bit 0x400 is unused by XInput's public
// mask) plus trigger pseudo-buttons above the 16-bit range, so triggers can
// be bound like buttons. Guarded — key_mappings.h defines them identically.
#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE         0x400
#endif
#ifndef XINPUT_GAMEPAD_LEFT_TRIGGER
#define XINPUT_GAMEPAD_LEFT_TRIGGER  0x10000
#endif
#ifndef XINPUT_GAMEPAD_RIGHT_TRIGGER
#define XINPUT_GAMEPAD_RIGHT_TRIGGER 0x20000
#endif

// XInput tuning constants (provided by XInput.h on Windows).
#ifndef XINPUT_GAMEPAD_TRIGGER_THRESHOLD
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30
#endif
#ifndef XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#endif
#ifndef XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#endif

// Keybind types (guarded — key_mappings.h defines them identically).
#ifndef SWITCH
#define SWITCH 1
#endif
#ifndef TOGGLE
#define TOGGLE 2
#endif
#ifndef HOLD
#define HOLD   3
#endif
