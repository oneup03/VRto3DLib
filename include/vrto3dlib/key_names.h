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

// Portable key-name vocabulary for configs/profiles. Names map to the
// numeric currency in key_codes.h: keyboard/mouse names -> VK codes,
// gamepad names -> XINPUT_GAMEPAD_* bitmasks.
//
// Portable names: "Key_A".."Key_Z", "Key_0".."Key_9", "Key_F1".."Key_F24",
// "Numpad0".."Numpad9", "NumpadMultiply/Add/Subtract/Decimal/Divide",
// "Key_Backspace/Tab/Enter/Shift/Ctrl/Alt/Pause/CapsLock/Escape/Space/
//  PageUp/PageDown/End/Home/Left/Up/Right/Down/PrintScreen/Insert/Delete/
//  Minus/Equals/LeftBracket/RightBracket", "Mouse_Left/Right/Middle/4/5",
// "Pad_A/B/X/Y", "Pad_LB/RB/LT/RT", "Pad_Start/Back/Guide", "Pad_LS/RS",
// "Pad_DPadUp/DPadDown/DPadLeft/DPadRight".
//
// Legacy "VK_*" / "XINPUT_GAMEPAD_*" names still parse so existing configs
// keep working; MigrateName() rewrites them to the portable spelling.

#include <cstdint>
#include <string>

namespace vrto3d::keys {

int  KeyCodeFromName(const std::string& name);    // keyboard/mouse name -> VK code, -1 unknown
int  PadBitsFromName(const std::string& name);    // gamepad name -> XINPUT bitmask, -1 unknown
bool IsGamepadName(const std::string& name);

std::string NameFromKeyCode(int vk);              // canonical portable name, "" unknown
std::string NameFromPadBits(int bits);

bool IsLegacyName(const std::string& name);       // starts with "VK_" or "XINPUT_"
std::string MigrateName(const std::string& name); // legacy -> portable; passthrough otherwise

// Parse a keybind string (portable or legacy spelling; '+'-joined gamepad
// combos) into a numeric code and gamepad flag. Returns false if nothing
// resolved (code left 0). When migrate is true, name is rewritten in place to
// the canonical portable spelling. Single source of truth for both the
// profile loader and the OSD's live re-parse.
bool ParseBind(std::string& name, int32_t& code, bool& xinput, bool migrate = true);

// Keybind-type name ("switch"/"toggle"/"hold") -> SWITCH/TOGGLE/HOLD, or
// `fallback` when unrecognized.
int KeyBindTypeFromName(const std::string& name, int fallback = -1);

}  // namespace vrto3d::keys
