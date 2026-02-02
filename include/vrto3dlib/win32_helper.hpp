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

#define WIN32_LEAN_AND_MEAN
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_set>
#include <windows.h>
#include <XInput.h>

#include "vrto3dlib/debug_log.hpp"
#include "vrto3dlib/key_mappings.h"
#include "vrto3dlib/stereo_config.h"


 // Link the XInput library
#pragma comment(lib, "XInput.lib")

//-----------------------------------------------------------------------------
// Purpose:
// Set a function pointer to the xinput get state call. By default, set it to
// XInputGetState() in whichever xinput we are linked to (xinput9_1_0.dll). If
// the d3dx.ini is using the guide button we will try to switch to either
// xinput 1.3 or 1.4 to get access to the undocumented XInputGetStateEx() call.
// We can't rely on these existing on Win7 though, so if we fail to load them
// don't treat it as fatal and continue using the original one.
//-----------------------------------------------------------------------------
static HMODULE xinput_lib;
typedef DWORD(WINAPI* tXInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
static tXInputGetState _XInputGetState = XInputGetState;
static void SwitchToXinpuGetStateEx()
{
    tXInputGetState XInputGetStateEx;

    if (xinput_lib)
        return;

    // 3DMigoto is linked against xinput9_1_0.dll, but that version does
    // not export XInputGetStateEx to get the guide button. Try loading
    // xinput 1.3 and 1.4, which both support this functionality.
    xinput_lib = LoadLibrary(L"xinput1_3.dll");
    if (xinput_lib) {
        LOG() << "Loaded xinput1_3.dll for guide button support";
    }
    else {
        xinput_lib = LoadLibrary(L"xinput1_4.dll");
        if (xinput_lib) {
            LOG() << "Loaded xinput1_4.dll for guide button support";
        }
        else {
            LOG() << "ERROR: Unable to load xinput 1.3 or 1.4: Guide button will not be available";
            return;
        }
    }

    // Unnamed and undocumented exports FTW
    LPCSTR XInputGetStateExOrdinal = (LPCSTR)100;
    XInputGetStateEx = (tXInputGetState)GetProcAddress(xinput_lib, XInputGetStateExOrdinal);
    if (!XInputGetStateEx) {
        LOG() << "ERROR: Unable to get XInputGetStateEx: Guide button will not be available";
        return;
    }

    _XInputGetState = XInputGetStateEx;
}


//-----------------------------------------------------------------------------
// Purpose: Get XInput button state including triggers as buttons
//-----------------------------------------------------------------------------
inline bool GetXInputButtonState(DWORD& outButtons, DWORD userIndex = 0)
{
    XINPUT_STATE state{};
    if (_XInputGetState(userIndex, &state) != ERROR_SUCCESS) {
        outButtons = 0;
        return false;
    }

    DWORD buttons = state.Gamepad.wButtons;

    if (state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= XINPUT_GAMEPAD_LEFT_TRIGGER;

    if (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
        buttons |= XINPUT_GAMEPAD_RIGHT_TRIGGER;

    outButtons = buttons;
    return true;
}


//-----------------------------------------------------------------------------
// Purpose: Cleaner check for key down state
//-----------------------------------------------------------------------------
inline auto isDown = [](int vk) { return GetAsyncKeyState(vk) & 0x8000; };
inline auto isCtrlDown = [&]() { return isDown(VK_CONTROL); };


//-----------------------------------------------------------------------------
// Purpose: Signify Operation Success
//-----------------------------------------------------------------------------
static void BeepSuccess()
{
    // High beep for success
    Beep(400, 400);
}


//-----------------------------------------------------------------------------
// Purpose: Signify Operation Failure
//-----------------------------------------------------------------------------
static void BeepFailure()
{
    // Brnk, dunk sound for failure.
    Beep(300, 200); Beep(200, 150);
}


//-----------------------------------------------------------------------------
// Purpose: Check if two floats are nearly equal within a max delta
//-----------------------------------------------------------------------------
inline bool NearlyEqual(float a, float b, float maxDelta = 0.001f)
{
    return std::fabs(a - b) <= maxDelta;
}


//-----------------------------------------------------------------------------
// Purpose: Check and apply user settings hotkeys
//-----------------------------------------------------------------------------
struct DepthConvBackend
{
    float (*getDepth)(void* ctx);
    float (*getConv)(void* ctx);
    void  (*setDepth)(void* ctx, float v);
    void  (*setConv)(void* ctx, float v);

    void (*onApplied)(void* ctx) = nullptr; // optional
    void* ctx = nullptr;
};

inline std::string ApplyUserSettingsHotkeys(
    StereoDisplayDriverConfiguration& cfg,
    bool got_xinput,
    DWORD xstate,
    const DepthConvBackend& b,
    float maxDelta = 0.001f)
{
    std::string storeMsg;

    auto applied = [&]() {
        if (b.onApplied) b.onApplied(b.ctx);
    };

    for (size_t i = 0; i < cfg.num_user_settings; ++i)
    {
        if (cfg.sleep_count[i] > 0)
            cfg.sleep_count[i]--;

        const bool loadPressed =
            (cfg.load_xinput[i] && got_xinput &&
                ((xstate & static_cast<DWORD>(cfg.user_load_key[i])) ==
                    static_cast<DWORD>(cfg.user_load_key[i])))
            || (!cfg.load_xinput[i] && isDown(cfg.user_load_key[i]));

        if (loadPressed)
        {
            const int32_t kt = cfg.user_key_type[i];

            if (kt == HOLD && !cfg.was_held[i])
            {
                cfg.prev_depth[i] = b.getDepth(b.ctx);
                cfg.prev_convergence[i] = b.getConv(b.ctx);
                cfg.was_held[i] = true;

                b.setDepth(b.ctx, cfg.user_depth[i]);
                b.setConv(b.ctx, cfg.user_convergence[i]);
                applied();
            }
            else if (kt == TOGGLE && cfg.sleep_count[i] < 1)
            {
                cfg.sleep_count[i] = cfg.sleep_count_max;

                const float curD = b.getDepth(b.ctx);
                const float curC = b.getConv(b.ctx);

                const bool matches =
                    NearlyEqual(curD, cfg.user_depth[i], maxDelta) &&
                    NearlyEqual(curC, cfg.user_convergence[i], maxDelta);

                if (matches)
                {
                    b.setDepth(b.ctx, cfg.prev_depth[i]);
                    b.setConv(b.ctx, cfg.prev_convergence[i]);
                }
                else
                {
                    cfg.prev_depth[i] = curD;
                    cfg.prev_convergence[i] = curC;

                    b.setDepth(b.ctx, cfg.user_depth[i]);
                    b.setConv(b.ctx, cfg.user_convergence[i]);
                }
                applied();
            }
            else if (kt == SWITCH)
            {
                b.setDepth(b.ctx, cfg.user_depth[i]);
                b.setConv(b.ctx, cfg.user_convergence[i]);
                applied();
            }
        }
        else if (cfg.user_key_type[i] == HOLD && cfg.was_held[i])
        {
            cfg.was_held[i] = false;
            b.setDepth(b.ctx, cfg.prev_depth[i]);
            b.setConv(b.ctx, cfg.prev_convergence[i]);
            applied();
        }

        // Store hotkey
        if (isDown(cfg.user_store_key[i]))
        {
            cfg.user_depth[i] = b.getDepth(b.ctx);
            cfg.user_convergence[i] = b.getConv(b.ctx);
            BeepSuccess();
            storeMsg = "Hotkey " + cfg.user_load_str[i] + " updated";
        }
    }

    return storeMsg;
}


 //-----------------------------------------------------------------------------
 // Purpose: Force focus to a specific window
 //-----------------------------------------------------------------------------
inline void ForceFocus(HWND hTarget, DWORD currentThread, DWORD targetThread) {
    // Send dummy input to enable focus control
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_MENU;
    SendInput(1, &input, sizeof(INPUT));
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));  // let input register

    // Attach input threads so you can manipulate the foreground window
    AttachThreadInput(currentThread, targetThread, TRUE);
    ShowWindow(hTarget, SW_RESTORE);
    SetForegroundWindow(hTarget);
    SetFocus(hTarget);
    SetActiveWindow(hTarget);
    BringWindowToTop(hTarget);
    AttachThreadInput(currentThread, targetThread, FALSE);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}


//-----------------------------------------------------------------------------
// Purpose: To get the executable name given a process ID
//-----------------------------------------------------------------------------
inline std::string GetProcessName(uint32_t processID)
{
    std::string result = "<unknown>";

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (hProcess)
    {
        TCHAR processName[MAX_PATH] = TEXT("<unknown>");
        DWORD size = MAX_PATH;

        // Try QueryFullProcessImageName for better accuracy and access support
        if (QueryFullProcessImageName(hProcess, 0, processName, &size))
        {
#ifdef UNICODE
            std::wstring ws(processName);
            result.assign(ws.begin(), ws.end());
#else
            result.assign(processName);
#endif
            std::filesystem::path fullPath = result;
            result = fullPath.filename().string();
        }

        CloseHandle(hProcess);
    }

    return result;
}


//-----------------------------------------------------------------------------
// Purpose: Return window handle from process ID
//-----------------------------------------------------------------------------
inline HWND GetHWNDFromPID(DWORD targetPID) {
    struct FindWindowData {
        DWORD targetPID;
        HWND result;
    } data = { targetPID, nullptr };

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* pData = reinterpret_cast<FindWindowData*>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);

        if (pid == pData->targetPID && IsWindowVisible(hwnd)) {
            pData->result = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&data));

    return data.result;
}


//-----------------------------------------------------------------------------
// Purpose: Check if a game is still running
//-----------------------------------------------------------------------------
inline bool IsProcessRunning(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    DWORD exitCode = 0;
    bool isRunning = false;

    if (GetExitCodeProcess(hProcess, &exitCode)) {
        isRunning = (exitCode == STILL_ACTIVE);
    }

    CloseHandle(hProcess);
    return isRunning;
}


//-----------------------------------------------------------------------------
// Purpose: Avoid profiling certain processes
//-----------------------------------------------------------------------------
static const std::unordered_set<std::string> Skip_Processes = {
        "vrcompositor.exe",
        "vrserver.exe",
        "vrmonitor.exe",
        "vrstartup.exe",
        "removeusbhelper.exe",
        "restarthelper.exe",
        "vrcmd.exe",
        "vrdashboard.exe",
        "vrpathreg.exe",
        "vrwebhelper.exe",
        "vrprismhost.exe",
        "vrserverhelper.exe",
        "vrservice.exe",
        "vrurlhandler.exe",
        "steam.exe",
        "steamwebhelper.exe",
        "steamerrorreporter.exe",
        "steamservice.exe",
        "ReviveOverlay.exe",
        "ReviveInjector.exe",
        "Rundll32.exe",
        "Rundll64.exe",
        "fpsVR.exe",
        "Driver4VR.exe"
};


//-----------------------------------------------------------------------------
// Purpose: RealVR Mutex Name
//-----------------------------------------------------------------------------
static constexpr const wchar_t* kVRto3DMutexName = L"Global\\VRto3DDriver";
