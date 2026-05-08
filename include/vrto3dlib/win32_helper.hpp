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
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
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
// Purpose: Replace forward slashes and remove trailing slashes
//-----------------------------------------------------------------------------
inline std::string NormalizeSteamPath(std::string p) {
    for (auto& c : p) if (c == '/') c = '\\';
    while (!p.empty() && (p.back() == '\\' || p.back() == '/')) p.pop_back();
    return p;
}


//-----------------------------------------------------------------------------
// Purpose: Convert UTF-16 strings to UTF-8 safely
//-----------------------------------------------------------------------------
inline std::string WideToUtf8(const wchar_t* wide)
{
    if (!wide || *wide == L'\0') {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (required_size <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(required_size), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        wide,
        -1,
        utf8.data(),
        required_size,
        nullptr,
        nullptr);

    if (converted <= 0) {
        return {};
    }

    if (!utf8.empty() && utf8.back() == '\0') {
        utf8.pop_back();
    }

    return utf8;
}


//-----------------------------------------------------------------------------
// Purpose: Retrieve Steam path from registry
//-----------------------------------------------------------------------------
inline std::string GetSteamInstallPath() {
    HKEY hKey;
    const char* subKey = "SOFTWARE\\Valve\\Steam";
    char steamPath[MAX_PATH] = {};
    DWORD steamPathSize = sizeof(steamPath);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)steamPath, &steamPathSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return NormalizeSteamPath(std::string(steamPath));
        }
        RegCloseKey(hKey);
    }
    OutputDebugStringA("VRto3D: Failed to find Steam install path from registry.\n");
    return "";
}


//-----------------------------------------------------------------------------
// Purpose: Write OpenXR ActiveRuntime for a specific registry hive
//-----------------------------------------------------------------------------
inline bool SetOpenXRActiveRuntimeForHive(HKEY hive, const char* hive_name, const std::string& runtime_json_path)
{
    const char* openxr_sub_key = "SOFTWARE\\Khronos\\OpenXR\\1";
    HKEY key = nullptr;

    REGSAM access_mask = KEY_SET_VALUE;
    if (hive == HKEY_LOCAL_MACHINE) {
        access_mask |= KEY_WOW64_64KEY;
    }

    const LONG create_result = RegCreateKeyExA(
        hive,
        openxr_sub_key,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        access_mask,
        nullptr,
        &key,
        nullptr);

    if (create_result != ERROR_SUCCESS) {
        LOG() << "Failed to open/create OpenXR key in " << hive_name
            << " (error=" << static_cast<int>(create_result) << ")";
        return false;
    }

    const DWORD data_size = static_cast<DWORD>(runtime_json_path.size() + 1);
    const LONG set_result = RegSetValueExA(
        key,
        "ActiveRuntime",
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(runtime_json_path.c_str()),
        data_size);

    RegCloseKey(key);

    if (set_result != ERROR_SUCCESS) {
        LOG() << "Failed to set OpenXR ActiveRuntime in " << hive_name
            << " (error=" << static_cast<int>(set_result) << ")";
        return false;
    }

    LOG() << "Set OpenXR ActiveRuntime in " << hive_name << " to: " << runtime_json_path.c_str();
    return true;
}


//-----------------------------------------------------------------------------
// Purpose: Set OpenXR runtime to SteamVR
//-----------------------------------------------------------------------------
inline bool SetOpenXRRuntimeToSteamVR()
{
    const std::string steam_install_path = GetSteamInstallPath();
    if (steam_install_path.empty()) {
        LOG() << "Cannot set OpenXR runtime: Steam install path was not found.";
        return false;
    }

    const std::filesystem::path runtime_json_path =
        std::filesystem::path(steam_install_path) / "steamapps" / "common" / "SteamVR" / "steamxr_win64.json";

    if (!std::filesystem::exists(runtime_json_path)) {
        LOG() << "Cannot set OpenXR runtime: SteamVR runtime json not found at: "
            << runtime_json_path.string().c_str();
        return false;
    }

    const std::string normalized_runtime_path = NormalizeSteamPath(runtime_json_path.string());
    LOG() << "Attempting to set OpenXR ActiveRuntime to SteamVR runtime: " << normalized_runtime_path.c_str();

    if (SetOpenXRActiveRuntimeForHive(HKEY_CURRENT_USER, "HKCU", normalized_runtime_path)) {
        return true;
    }

    LOG() << "HKCU OpenXR ActiveRuntime update failed. Trying HKLM fallback.";
    if (SetOpenXRActiveRuntimeForHive(HKEY_LOCAL_MACHINE, "HKLM", normalized_runtime_path)) {
        return true;
    }

    LOG() << "Failed to set OpenXR ActiveRuntime to SteamVR in both HKCU and HKLM.";
    return false;
}


//-----------------------------------------------------------------------------
// Purpose: Check if two floats are nearly equal within a max delta
//-----------------------------------------------------------------------------
inline bool NearlyEqual(float a, float b, float maxDelta = 0.001f)
{
    return std::fabs(a - b) <= maxDelta;
}


//-----------------------------------------------------------------------------
// Purpose: Display monitor bounds helpers
//-----------------------------------------------------------------------------
struct MonitorBounds
{
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool is_primary = false;
    int32_t display_index = 0; // Monitor enumeration order (1-based)
    std::string device_name;
};


inline BOOL CALLBACK EnumMonitorBoundsProc(HMONITOR monitor, HDC, LPRECT, LPARAM user_data)
{
    auto* monitors = reinterpret_cast<std::vector<MonitorBounds>*>(user_data);
    MONITORINFOEXA info{};
    info.cbSize = sizeof(info);

    if (!GetMonitorInfoA(monitor, &info)) {
        return TRUE;
    }

    MonitorBounds bounds{};
    bounds.x = info.rcMonitor.left;
    bounds.y = info.rcMonitor.top;
    bounds.width = static_cast<uint32_t>(info.rcMonitor.right - info.rcMonitor.left);
    bounds.height = static_cast<uint32_t>(info.rcMonitor.bottom - info.rcMonitor.top);
    bounds.is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
    bounds.device_name = info.szDevice;
    bounds.display_index = static_cast<int32_t>(monitors->size()) + 1;
    monitors->push_back(bounds);

    return TRUE;
}


inline bool ResolveMonitorBoundsByDisplayIndex(
    int32_t requested_display_index,
    MonitorBounds& out_bounds,
    bool& used_primary_fallback,
    bool& used_primary_default,
    std::vector<MonitorBounds>* out_monitors = nullptr)
{
    used_primary_fallback = false;
    used_primary_default = false;

    std::vector<MonitorBounds> monitors;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonitorBoundsProc, reinterpret_cast<LPARAM>(&monitors));

    if (monitors.empty()) {
        return false;
    }

    if (out_monitors != nullptr) {
        *out_monitors = monitors;
    }

    LOG() << "Detected " << static_cast<int>(monitors.size()) << " active monitor(s)";
    for (const auto& monitor : monitors) {
        LOG()
            << "Display order " << monitor.display_index
            << " (" << monitor.device_name.c_str() << ")"
            << (monitor.is_primary ? " [primary]" : "")
            << " bounds=(" << monitor.x << "," << monitor.y << " " << monitor.width << "x" << monitor.height << ")";
    }

    auto pick_primary = [&]() -> bool {
        for (const auto& monitor : monitors) {
            if (monitor.is_primary) {
                out_bounds = monitor;
                return true;
            }
        }
        return false;
    };

    if (requested_display_index <= 0) {
        used_primary_default = pick_primary();
        return used_primary_default;
    }

    const int32_t monitor_count = static_cast<int32_t>(monitors.size());
    if (requested_display_index <= monitor_count) {
        out_bounds = monitors[static_cast<size_t>(requested_display_index - 1)];
        return true;
    }

    used_primary_fallback = pick_primary();
    return used_primary_fallback;
}


inline bool ApplyDisplaySelectionToWindowConfig(StereoDisplayDriverConfiguration& config)
{
    std::vector<MonitorBounds> monitors;
    MonitorBounds selected{};
    bool used_primary_fallback = false;
    bool used_primary_default = false;

    if (!ResolveMonitorBoundsByDisplayIndex(
        config.display_index,
        selected,
        used_primary_fallback,
        used_primary_default,
        &monitors)) {
        LOG() << "Failed to resolve monitor bounds. Keeping existing window bounds.";
        return false;
    }

    config.window_x = selected.x;
    config.window_y = selected.y;
    config.window_width = selected.width;
    config.window_height = selected.height;

    if (used_primary_default) {
        LOG()
            << "display_index=0 (auto). Using primary display order " << selected.display_index
            << " (" << selected.device_name.c_str() << ") for window bounds ("
            << selected.x << "," << selected.y << " " << selected.width << "x" << selected.height << ")";
    }
    else if (used_primary_fallback) {
        LOG()
            << "Configured display_index=" << config.display_index
            << " is unavailable. Falling back to primary display order " << selected.display_index
            << " (" << selected.device_name.c_str() << ") for window bounds ("
            << selected.x << "," << selected.y << " " << selected.width << "x" << selected.height << ")";
    }
    else {
        LOG()
            << "Using display_index=" << config.display_index
            << " (display order " << selected.display_index << ", " << selected.device_name.c_str() << ") for window bounds ("
            << selected.x << "," << selected.y << " " << selected.width << "x" << selected.height << ")";
    }
    return true;
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
    float (*getFov)(void* ctx);
    void  (*setFov)(void* ctx, float v);

    void (*onApplied)(void* ctx) = nullptr; // optional (e.g. mark dirty)
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

        if (cfg.user_depth[i].empty()) continue;  // malformed row, skip

        // Bounds-clamp the cycle index in case the preset list shrunk.
        if (i >= cfg.user_preset_index.size()) cfg.user_preset_index.resize(i + 1, 0);
        if (cfg.user_preset_index[i] >= cfg.user_depth[i].size())
            cfg.user_preset_index[i] = 0;

        const size_t presets = cfg.user_depth[i].size();
        size_t idx = cfg.user_preset_index[i];

        // HOLD ignores cycling — it just toggles preset[0] in/out.
        const size_t hold_idx = 0;

        // Helpers to fetch a preset's values; tolerates short conv/fov vectors.
        auto getD = [&](size_t k) { return cfg.user_depth[i][k]; };
        auto getC = [&](size_t k) {
            return k < cfg.user_convergence[i].size()
                 ? cfg.user_convergence[i][k]
                 : (cfg.user_convergence[i].empty() ? 1.0f : cfg.user_convergence[i].back());
        };
        auto getF = [&](size_t k) {
            float f = k < cfg.user_fov[i].size()
                    ? cfg.user_fov[i][k]
                    : (cfg.user_fov[i].empty() ? 0.0f : cfg.user_fov[i].back());
            // FoV == 0 in the user-hotkey row is the documented sentinel for
            // "don't override — keep the active profile FoV". Falls back to
            // the live cfg.fov so the press doesn't snap the FoV to zero.
            if (f <= 0.0f) f = cfg.fov;
            return f;
        };
        auto applyPreset = [&](size_t k) {
            b.setDepth(b.ctx, getD(k));
            b.setConv(b.ctx,  getC(k));
            b.setFov(b.ctx,   getF(k));
            applied();
        };

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
                cfg.prev_fov[i] = b.getFov(b.ctx);
                cfg.was_held[i] = true;
                applyPreset(hold_idx);
            }
            else if (kt == TOGGLE && cfg.sleep_count[i] < 1)
            {
                cfg.sleep_count[i] = cfg.sleep_count_max;

                const float curD = b.getDepth(b.ctx);
                const float curC = b.getConv(b.ctx);
                const float curF = b.getFov(b.ctx);

                // If we currently match this preset, advance to the next; if
                // we cycle back to where we started, revert to prev.
                const bool matches =
                    NearlyEqual(curD, getD(idx), maxDelta) &&
                    NearlyEqual(curC, getC(idx), maxDelta) &&
                    NearlyEqual(curF, getF(idx), maxDelta);

                if (matches && presets > 1) {
                    idx = (idx + 1) % presets;
                    cfg.user_preset_index[i] = idx;
                    applyPreset(idx);
                } else if (matches) {
                    b.setDepth(b.ctx, cfg.prev_depth[i]);
                    b.setConv(b.ctx,  cfg.prev_convergence[i]);
                    b.setFov(b.ctx,   cfg.prev_fov[i]);
                    applied();
                } else {
                    cfg.prev_depth[i] = curD;
                    cfg.prev_convergence[i] = curC;
                    cfg.prev_fov[i] = curF;
                    applyPreset(idx);
                }
            }
            else if (kt == SWITCH && cfg.sleep_count[i] < 1)
            {
                cfg.sleep_count[i] = cfg.sleep_count_max;
                applyPreset(idx);
                cfg.user_preset_index[i] = (idx + 1) % presets;
            }
        }
        else if (cfg.user_key_type[i] == HOLD && cfg.was_held[i])
        {
            cfg.was_held[i] = false;

            b.setDepth(b.ctx, cfg.prev_depth[i]);
            b.setConv(b.ctx, cfg.prev_convergence[i]);
            b.setFov(b.ctx, cfg.prev_fov[i]);
            applied();
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
            result = WideToUtf8(processName);
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
// Entries are lowercase — caller compares against the lowered process name so
// the match is case-insensitive (vendors occasionally ship the same exe with
// different casing across versions).
static const std::unordered_set<std::string> Skip_Processes = {
        // SteamVR / Steam infrastructure
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
        "rundll32.exe",
        "rundll64.exe",

        // Existing third-party utilities
        "reviveoverlay.exe",
        "reviveinjector.exe",
        "fpsvr.exe",
        "driver4vr.exe",

        // PSVR2 SteamVR driver helpers
        "psvr2_dialog.exe",
        "psvr2_overlay.exe",

        // HTC / Vive driver helpers
        "lhrreceiverserver.exe",
        "rrconsole.exe",
        "htcconnectionutility.exe",
        "vivestreamingclient.exe",
        "vivestreamingserver.exe",

        // Oculus / Meta runtime
        "ovrserver_x64.exe",
        "ovrservicelauncher.exe",
        "ovrredir.exe",
        "oculusclient.exe",
        "oculusdash.exe",

        // Pimax
        "pitool.exe",
        "piserver.exe",
        "piservicelauncher.exe",
        "pvrclient.exe",

        // Varjo
        "varjobase.exe",
        "varjocompositor.exe",
        "varjosession.exe",

        // Windows Mixed Reality
        "mixedrealityportal.exe",
        "holoshellapp.exe",
        "wmrgfxmonitor.exe",

        // ALVR / WiVRn
        "alvr_dashboard.exe",
        "alvr.exe",
        "wivrn.exe",

        // Virtual Desktop
        "virtualdesktop.streamer.exe",
        "virtualdesktop.service.exe",

        // Pico (Streaming Assistant / PICO Connect)
        "streaming assistant.exe",
        "picovr streaming assistant.exe",
        "ps_server.exe",

        // Shiftall MeganeX
        "custom-headset-gui.exe",

        // Always-on overlay apps that connect to SteamVR
        "liv.exe",
        "xsoverlay.exe",
        "ovrtoolkit.exe",
        "oyasumi.exe",
        "ovradvancedsettings.exe",
        "advancedsettings.exe",
        "openvr-spacecalibrator.exe",
        "openvr-spacecalibratorex.exe",
        "bhapticsplayer.exe",
        "slimevr-server.exe",
        "vrcfacetracking.exe",
        "eyetrackvr.exe"
};


//-----------------------------------------------------------------------------
// Purpose: RealVR Mutex Name
//-----------------------------------------------------------------------------
static constexpr const wchar_t* kVRto3DMutexName = L"Global\\VRto3DDriver";
