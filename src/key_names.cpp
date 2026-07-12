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

#include "vrto3dlib/key_names.h"
#include "vrto3dlib/key_codes.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <unordered_map>

namespace vrto3d::keys {

namespace {

//-----------------------------------------------------------------------------
// Purpose: Portable keyboard/mouse name -> VK code (canonical vocabulary)
//-----------------------------------------------------------------------------
const std::unordered_map<std::string, int>& PortableKeyTable()
{
    static const std::unordered_map<std::string, int> table = [] {
        std::unordered_map<std::string, int> t = {
            {"Key_Backspace",    VK_BACK},
            {"Key_Tab",          VK_TAB},
            {"Key_Enter",        VK_RETURN},
            {"Key_Shift",        VK_SHIFT},
            {"Key_Ctrl",         VK_CONTROL},
            {"Key_Alt",          VK_MENU},
            {"Key_Pause",        VK_PAUSE},
            {"Key_CapsLock",     VK_CAPITAL},
            {"Key_Escape",       VK_ESCAPE},
            {"Key_Space",        VK_SPACE},
            {"Key_PageUp",       VK_PRIOR},
            {"Key_PageDown",     VK_NEXT},
            {"Key_End",          VK_END},
            {"Key_Home",         VK_HOME},
            {"Key_Left",         VK_LEFT},
            {"Key_Up",           VK_UP},
            {"Key_Right",        VK_RIGHT},
            {"Key_Down",         VK_DOWN},
            {"Key_PrintScreen",  VK_SNAPSHOT},
            {"Key_Insert",       VK_INSERT},
            {"Key_Delete",       VK_DELETE},
            {"Key_Minus",        VK_OEM_MINUS},
            {"Key_Equals",       VK_OEM_PLUS},
            {"Key_LeftBracket",  VK_OEM_4},
            {"Key_RightBracket", VK_OEM_6},
            {"NumpadMultiply",   VK_MULTIPLY},
            {"NumpadAdd",        VK_ADD},
            {"NumpadSubtract",   VK_SUBTRACT},
            {"NumpadDecimal",    VK_DECIMAL},
            {"NumpadDivide",     VK_DIVIDE},
            {"Mouse_Left",       VK_LBUTTON},
            {"Mouse_Right",      VK_RBUTTON},
            {"Mouse_Middle",     VK_MBUTTON},
            {"Mouse_4",          VK_XBUTTON1},
            {"Mouse_5",          VK_XBUTTON2},
        };
        for (int i = 0; i < 26; ++i) {
            t[std::string("Key_") + static_cast<char>('A' + i)] = 'A' + i;
        }
        for (int i = 0; i < 10; ++i) {
            t[std::string("Key_") + static_cast<char>('0' + i)] = '0' + i;
        }
        for (int i = 0; i < 24; ++i) {
            t["Key_F" + std::to_string(i + 1)] = VK_F1 + i;
        }
        for (int i = 0; i < 10; ++i) {
            t["Numpad" + std::to_string(i)] = VK_NUMPAD0 + i;
        }
        return t;
    }();
    return table;
}

//-----------------------------------------------------------------------------
// Purpose: Portable gamepad name -> XINPUT_GAMEPAD_* bitmask
//-----------------------------------------------------------------------------
const std::unordered_map<std::string, int>& PortablePadTable()
{
    static const std::unordered_map<std::string, int> table = {
        {"Pad_A",         XINPUT_GAMEPAD_A},
        {"Pad_B",         XINPUT_GAMEPAD_B},
        {"Pad_X",         XINPUT_GAMEPAD_X},
        {"Pad_Y",         XINPUT_GAMEPAD_Y},
        {"Pad_LB",        XINPUT_GAMEPAD_LEFT_SHOULDER},
        {"Pad_RB",        XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {"Pad_LT",        XINPUT_GAMEPAD_LEFT_TRIGGER},
        {"Pad_RT",        XINPUT_GAMEPAD_RIGHT_TRIGGER},
        {"Pad_Start",     XINPUT_GAMEPAD_START},
        {"Pad_Back",      XINPUT_GAMEPAD_BACK},
        {"Pad_Guide",     XINPUT_GAMEPAD_GUIDE},
        {"Pad_LS",        XINPUT_GAMEPAD_LEFT_THUMB},
        {"Pad_RS",        XINPUT_GAMEPAD_RIGHT_THUMB},
        {"Pad_DPadUp",    XINPUT_GAMEPAD_DPAD_UP},
        {"Pad_DPadDown",  XINPUT_GAMEPAD_DPAD_DOWN},
        {"Pad_DPadLeft",  XINPUT_GAMEPAD_DPAD_LEFT},
        {"Pad_DPadRight", XINPUT_GAMEPAD_DPAD_RIGHT},
    };
    return table;
}

//-----------------------------------------------------------------------------
// Purpose: Legacy keyboard/mouse names (the pre-portable "VK_*" vocabulary),
//          kept so older configs keep parsing and can be migrated
//-----------------------------------------------------------------------------
const std::unordered_map<std::string, int>& LegacyKeyTable()
{
    static const std::unordered_map<std::string, int> table = [] {
        std::unordered_map<std::string, int> t = {
            {"VK_LMOUSE",    VK_LBUTTON},
            {"VK_RMOUSE",    VK_RBUTTON},
            {"VK_MMOUSE",    VK_MBUTTON},
            {"VK_MOUSE4",    VK_XBUTTON1},
            {"VK_MOUSE5",    VK_XBUTTON2},
            {"VK_BACKSPACE", VK_BACK},
            {"VK_TAB",       VK_TAB},
            {"VK_RETURN",    VK_RETURN},
            {"VK_SHIFT",     VK_SHIFT},
            {"VK_CONTROL",   VK_CONTROL},
            {"VK_MENU",      VK_MENU},
            {"VK_PAUSE",     VK_PAUSE},
            {"VK_CAPS",      VK_CAPITAL},
            {"VK_ESCAPE",    VK_ESCAPE},
            {"VK_SPACE",     VK_SPACE},
            {"VK_PGUP",      VK_PRIOR},
            {"VK_PGDWN",     VK_NEXT},
            {"VK_END",       VK_END},
            {"VK_HOME",      VK_HOME},
            {"VK_LEFT",      VK_LEFT},
            {"VK_UP",        VK_UP},
            {"VK_RIGHT",     VK_RIGHT},
            {"VK_DOWN",      VK_DOWN},
            {"VK_SNAPSHOT",  VK_SNAPSHOT},
            {"VK_INSERT",    VK_INSERT},
            {"VK_DELETE",    VK_DELETE},
            {"VK_MULTIPLY",  VK_MULTIPLY},
            {"VK_ADD",       VK_ADD},
            {"VK_SUBTRACT",  VK_SUBTRACT},
            {"VK_DECIMAL",   VK_DECIMAL},
            {"VK_DIVIDE",    VK_DIVIDE},
            {"VK_OEM_MINUS", VK_OEM_MINUS},
            {"VK_OEM_PLUS",  VK_OEM_PLUS},
            {"VK_LBRACKET",  VK_OEM_4},
            {"VK_RBRACKET",  VK_OEM_6},
        };
        for (int i = 0; i < 26; ++i) {
            t[std::string("VK_") + static_cast<char>('A' + i)] = 'A' + i;
        }
        for (int i = 0; i < 10; ++i) {
            t[std::string("VK_") + static_cast<char>('0' + i)] = '0' + i;
        }
        for (int i = 0; i < 24; ++i) {
            t["VK_F" + std::to_string(i + 1)] = VK_F1 + i;
        }
        for (int i = 0; i < 10; ++i) {
            t["VK_NUMPAD" + std::to_string(i)] = VK_NUMPAD0 + i;
        }
        return t;
    }();
    return table;
}

//-----------------------------------------------------------------------------
// Purpose: Legacy gamepad names (the pre-portable "XINPUT_GAMEPAD_*" vocabulary)
//-----------------------------------------------------------------------------
const std::unordered_map<std::string, int>& LegacyPadTable()
{
    static const std::unordered_map<std::string, int> table = {
        {"XINPUT_GAMEPAD_A",              XINPUT_GAMEPAD_A},
        {"XINPUT_GAMEPAD_B",              XINPUT_GAMEPAD_B},
        {"XINPUT_GAMEPAD_X",              XINPUT_GAMEPAD_X},
        {"XINPUT_GAMEPAD_Y",              XINPUT_GAMEPAD_Y},
        {"XINPUT_GAMEPAD_RIGHT_SHOULDER", XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {"XINPUT_GAMEPAD_LEFT_SHOULDER",  XINPUT_GAMEPAD_LEFT_SHOULDER},
        {"XINPUT_GAMEPAD_LEFT_TRIGGER",   XINPUT_GAMEPAD_LEFT_TRIGGER},
        {"XINPUT_GAMEPAD_RIGHT_TRIGGER",  XINPUT_GAMEPAD_RIGHT_TRIGGER},
        {"XINPUT_GAMEPAD_DPAD_UP",        XINPUT_GAMEPAD_DPAD_UP},
        {"XINPUT_GAMEPAD_DPAD_DOWN",      XINPUT_GAMEPAD_DPAD_DOWN},
        {"XINPUT_GAMEPAD_DPAD_LEFT",      XINPUT_GAMEPAD_DPAD_LEFT},
        {"XINPUT_GAMEPAD_DPAD_RIGHT",     XINPUT_GAMEPAD_DPAD_RIGHT},
        {"XINPUT_GAMEPAD_START",          XINPUT_GAMEPAD_START},
        {"XINPUT_GAMEPAD_BACK",           XINPUT_GAMEPAD_BACK},
        {"XINPUT_GAMEPAD_GUIDE",          XINPUT_GAMEPAD_GUIDE},
        {"XINPUT_GAMEPAD_LEFT_THUMB",     XINPUT_GAMEPAD_LEFT_THUMB},
        {"XINPUT_GAMEPAD_RIGHT_THUMB",    XINPUT_GAMEPAD_RIGHT_THUMB},
    };
    return table;
}

//-----------------------------------------------------------------------------
// Purpose: Reverse lookups (code -> canonical portable name). Safe to build
//          from the forward tables: every portable name has a unique code.
//-----------------------------------------------------------------------------
const std::unordered_map<int, std::string>& KeyNameByCode()
{
    static const std::unordered_map<int, std::string> table = [] {
        std::unordered_map<int, std::string> t;
        for (const auto& [name, code] : PortableKeyTable()) {
            t.emplace(code, name);
        }
        return t;
    }();
    return table;
}

const std::unordered_map<int, std::string>& PadNameByBits()
{
    static const std::unordered_map<int, std::string> table = [] {
        std::unordered_map<int, std::string> t;
        for (const auto& [name, bits] : PortablePadTable()) {
            t.emplace(bits, name);
        }
        return t;
    }();
    return table;
}

int LookupOrMinusOne(const std::unordered_map<std::string, int>& table,
                     const std::string& name)
{
    const auto it = table.find(name);
    return it != table.end() ? it->second : -1;
}

}  // namespace

int KeyCodeFromName(const std::string& name)
{
    const int code = LookupOrMinusOne(PortableKeyTable(), name);
    return code != -1 ? code : LookupOrMinusOne(LegacyKeyTable(), name);
}

int PadBitsFromName(const std::string& name)
{
    const int bits = LookupOrMinusOne(PortablePadTable(), name);
    return bits != -1 ? bits : LookupOrMinusOne(LegacyPadTable(), name);
}

bool IsGamepadName(const std::string& name)
{
    return PadBitsFromName(name) != -1;
}

std::string NameFromKeyCode(int vk)
{
    const auto it = KeyNameByCode().find(vk);
    return it != KeyNameByCode().end() ? it->second : std::string();
}

std::string NameFromPadBits(int bits)
{
    const auto it = PadNameByBits().find(bits);
    return it != PadNameByBits().end() ? it->second : std::string();
}

bool IsLegacyName(const std::string& name)
{
    return name.rfind("VK_", 0) == 0 || name.rfind("XINPUT_", 0) == 0;
}

std::string MigrateName(const std::string& name)
{
    if (!IsLegacyName(name)) {
        return name;
    }
    int code = LookupOrMinusOne(LegacyKeyTable(), name);
    if (code != -1) {
        const std::string portable = NameFromKeyCode(code);
        return portable.empty() ? name : portable;
    }
    code = LookupOrMinusOne(LegacyPadTable(), name);
    if (code != -1) {
        const std::string portable = NameFromPadBits(code);
        return portable.empty() ? name : portable;
    }
    return name;  // unknown legacy name: leave for the caller to reject
}

bool ParseBind(std::string& name, int32_t& code, bool& xinput, bool migrate)
{
    code = 0;
    xinput = false;
    if (name.empty()) {
        return false;
    }

    const int key = KeyCodeFromName(name);
    if (key >= 0) {
        code = key;
        xinput = false;
        if (migrate) {
            name = MigrateName(name);
        }
        return true;
    }

    // A '+' means a gamepad chord; a bare gamepad name is the single-button case.
    if (PadBitsFromName(name) >= 0 || name.find('+') != std::string::npos) {
        std::string migrated;
        std::stringstream ss(name);
        std::string tok;
        while (std::getline(ss, tok, '+')) {
            const int bits = PadBitsFromName(tok);
            if (bits >= 0) {
                code |= bits;
                if (migrate) {
                    if (!migrated.empty()) migrated += '+';
                    migrated += MigrateName(tok);
                }
            }
        }
        if (code != 0) {
            xinput = true;
            if (migrate) {
                name = migrated;
            }
            return true;
        }
    }
    return false;
}

int KeyBindTypeFromName(const std::string& name, int fallback)
{
    static const std::unordered_map<std::string, int> table = {
        {"switch", SWITCH}, {"toggle", TOGGLE}, {"hold", HOLD},
    };
    const auto it = table.find(name);
    return it != table.end() ? it->second : fallback;
}

}  // namespace vrto3d::keys
