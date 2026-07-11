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

// Portable global-input API for the Linux port. Semantics mirror the Win32
// calls the driver uses (GetAsyncKeyState / XInputGetState / GetCursorPos):
// state is *global* — the driver lives in vrserver and the game window has
// focus, so a focused-window input model would never see anything.
//
// Linux backend: evdev (/dev/input/event*) reader thread with inotify
// hotplug. Requires the user to be in the `input` group; PermissionOk()
// reports whether any keyboard device could be opened.
//
// Key identity: plain int VK codes (see key_codes.h) — the same numeric
// currency the config/profile system stores. The backend translates VK ->
// evdev KEY_* internally.

#include <cstdint>

namespace vrto3d::input {

// Mirrors XINPUT_GAMEPAD field-for-field so shared driver math (head-look
// thread, hotkeys) works identically on both platforms.
struct GamepadState {
    bool     connected = false;
    uint16_t wButtons = 0;       // XINPUT_GAMEPAD_* bit constants
    uint8_t  bLeftTrigger = 0;   // 0..255
    uint8_t  bRightTrigger = 0;  // 0..255
    int16_t  sThumbLX = 0;       // -32768..32767, up/right positive
    int16_t  sThumbLY = 0;
    int16_t  sThumbRX = 0;
    int16_t  sThumbRY = 0;
};

struct MouseState {
    int32_t x = 0;               // virtual cursor, clamped to SetMouseRegion
    int32_t y = 0;
    int32_t wheel = 0;           // accumulated detents since last GetMouseState
    bool    left = false, right = false, middle = false, x1 = false, x2 = false;
};

// Edge events for the OSD (ImGui needs press/release edges + text).
struct KeyEvent {
    int  vk = 0;                 // 0 when the key has no VK mapping
    uint16_t evdev_code = 0;
    bool down = false;
};

// Start the evdev reader thread. Idempotent. Returns false when no input
// devices could be opened (permissions) — the driver keeps running, hotkeys
// are just dead; surface this to the user via OSD/log.
bool Start();
void Stop();
bool PermissionOk();

// Level-triggered state (hotkey polling).
bool IsKeyDown(int vk);
bool IsCtrlDown();               // VK_CONTROL (either side)

GamepadState GetGamepadState();  // merged across connected pads
MouseState   GetMouseState();
void SetMouseRegion(int32_t width, int32_t height);
void WarpMouse(int32_t x, int32_t y);

// Edge-triggered drain for the OSD input pump. Returns count written.
int  DrainKeyEvents(KeyEvent* out, int max_events);
// Drains UTF-8 text typed since last call (xkbcommon-translated). Returns
// bytes written (NUL-terminated).
int  DrainTypedUtf8(char* out, int out_size);

}  // namespace vrto3d::input
