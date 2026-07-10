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

// Linux counterpart of win32_helper.hpp: provides the same-named helpers the
// shared driver TUs use, implemented over /proc, POSIX signals, and the evdev
// input backend (vrto3dlib/input_state.h). Include this INSTEAD of
// win32_helper.hpp on non-Windows — call sites gate on _WIN32.

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vrto3dlib/debug_log.hpp"
#include "vrto3dlib/input_state.h"
#include "vrto3dlib/key_codes.h"
#include "vrto3dlib/hotkey_eval.hpp"
#include "vrto3dlib/stereo_config.h"

//-----------------------------------------------------------------------------
// Input state (evdev-backed mirrors of GetAsyncKeyState / XInputGetState)
//-----------------------------------------------------------------------------
inline auto isDown = [](int vk) { return vrto3d::input::IsKeyDown(vk); };
inline auto isCtrlDown = []() { return vrto3d::input::IsCtrlDown(); };

inline bool GetXInputButtonState(uint32_t& outButtons, uint32_t userIndex = 0)
{
    (void)userIndex;  // evdev backend merges all pads
    const auto pad = vrto3d::input::GetGamepadState();
    if (!pad.connected) {
        outButtons = 0;
        return false;
    }
    uint32_t buttons = pad.wButtons;
    if (pad.bLeftTrigger > 30)   // XINPUT_GAMEPAD_TRIGGER_THRESHOLD
        buttons |= XINPUT_GAMEPAD_LEFT_TRIGGER;
    if (pad.bRightTrigger > 30)
        buttons |= XINPUT_GAMEPAD_RIGHT_TRIGGER;
    outButtons = buttons;
    return true;
}

inline void BeepSuccess() {}
inline void BeepFailure() {}

inline bool NearlyEqual(float a, float b, float maxDelta = 0.001f)
{
    float diff = a - b;
    if (diff < 0) diff = -diff;
    return diff <= maxDelta;
}

//-----------------------------------------------------------------------------
// Steam install path: ~/.steam/root symlink -> ~/.steam/steam ->
// ~/.local/share/Steam -> $STEAM_DIR override.
//-----------------------------------------------------------------------------
inline std::string GetSteamInstallPath()
{
    namespace fs = std::filesystem;
    const char* home = getenv("HOME");
    std::vector<std::string> candidates;
    if (const char* env = getenv("STEAM_DIR"))
        candidates.push_back(env);
    if (home) {
        candidates.push_back(std::string(home) + "/.steam/root");
        candidates.push_back(std::string(home) + "/.steam/steam");
        candidates.push_back(std::string(home) + "/.local/share/Steam");
    }
    for (const auto& c : candidates) {
        std::error_code ec;
        fs::path p = fs::canonical(c, ec);   // resolves the ~/.steam symlinks
        if (!ec && fs::is_directory(p / "config", ec) && !ec)
            return p.string();
    }
    return {};
}

//-----------------------------------------------------------------------------
// OpenXR active runtime -> SteamVR (~/.config/openxr/1/active_runtime.json)
//-----------------------------------------------------------------------------
inline bool SetOpenXRRuntimeToSteamVR()
{
    namespace fs = std::filesystem;
    const std::string steam = GetSteamInstallPath();
    if (steam.empty())
        return false;
    const fs::path runtime_json =
        fs::path(steam) / "steamapps/common/SteamVR/steamxr_linux64.json";
    std::error_code ec;
    if (!fs::exists(runtime_json, ec))
        return false;

    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    fs::path cfg_dir = xdg ? fs::path(xdg) : (home ? fs::path(home) / ".config" : fs::path());
    if (cfg_dir.empty())
        return false;
    fs::path openxr_dir = cfg_dir / "openxr" / "1";
    fs::create_directories(openxr_dir, ec);
    fs::path active = openxr_dir / "active_runtime.json";

    // Already pointing at SteamVR? Leave it alone.
    fs::path current = fs::read_symlink(active, ec);
    if (!ec && current == runtime_json)
        return true;

    // Back up a pre-existing non-SteamVR runtime file once.
    if (fs::exists(active, ec) && !fs::exists(openxr_dir / "active_runtime.json.vrto3d_backup", ec))
        fs::copy_file(active, openxr_dir / "active_runtime.json.vrto3d_backup",
                      fs::copy_options::overwrite_existing, ec);

    fs::remove(active, ec);
    fs::create_symlink(runtime_json, active, ec);
    if (ec) {  // fall back to copying the manifest
        ec.clear();
        fs::copy_file(runtime_json, active, fs::copy_options::overwrite_existing, ec);
    }
    return !ec;
}

//-----------------------------------------------------------------------------
// Process helpers over /proc
//-----------------------------------------------------------------------------
inline std::string GetProcessName(uint32_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u/comm", pid);
    std::ifstream f(path);
    std::string name;
    if (f)
        std::getline(f, name);
    return name;
}

inline bool IsProcessRunning(uint32_t pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%u", pid);
    struct stat st{};
    return stat(path, &st) == 0;
}

// True when a process with the given comm name exists. `name` may carry a
// Windows-style ".exe" suffix — it is compared with and without it so the
// same call sites work for native and Proton processes.
inline bool IsProcessNameRunning(const char* name)
{
    std::string wanted = name;
    std::string wanted_noexe = wanted;
    const auto pos = wanted_noexe.rfind(".exe");
    if (pos != std::string::npos && pos == wanted_noexe.size() - 4)
        wanted_noexe.resize(pos);

    DIR* dir = opendir("/proc");
    if (!dir)
        return false;
    bool found = false;
    while (dirent* e = readdir(dir)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9')
            continue;
        char path[NAME_MAX + 16];
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        std::ifstream f(path);
        std::string comm;
        if (f && std::getline(f, comm)) {
            if (comm == wanted || comm == wanted_noexe) {
                found = true;
                break;
            }
        }
    }
    closedir(dir);
    return found;
}

inline void KillProcessesNamed(const char* comm_name, int sig)
{
    DIR* dir = opendir("/proc");
    if (!dir)
        return;
    while (dirent* e = readdir(dir)) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9')
            continue;
        char path[NAME_MAX + 16];
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        std::ifstream f(path);
        std::string comm;
        if (f && std::getline(f, comm) && comm == comm_name)
            kill((pid_t)atoi(e->d_name), sig);
    }
    closedir(dir);
}

//-----------------------------------------------------------------------------
// SteamVR shutdown: SIGTERM vrmonitor, wait, escalate; then vrserver if it
// lingers. Mirrors the taskkill flow in win32_helper.hpp.
//-----------------------------------------------------------------------------
inline void RequestSteamVRShutdown(int graceful_timeout_seconds = 5)
{
    const int poll_ms = 500;
    auto kill_named = [&](const char* comm) {
        KillProcessesNamed(comm, SIGTERM);
        const int max_polls = (graceful_timeout_seconds * 1000) / poll_ms;
        for (int i = 0; i < max_polls; ++i) {
            if (!IsProcessNameRunning(comm)) {
                LOG() << "auto_exit: " << comm << " exited cleanly after "
                      << (i * poll_ms) << "ms";
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
        }
        LOG() << "auto_exit: " << comm << " still running after "
              << graceful_timeout_seconds << "s, escalating to SIGKILL";
        KillProcessesNamed(comm, SIGKILL);
    };

    kill_named("vrmonitor");
    if (IsProcessNameRunning("vrserver")) {
        LOG() << "auto_exit: vrserver still running after vrmonitor exit, "
                 "closing it directly";
        kill_named("vrserver");
    }
}

inline std::atomic<uint32_t> g_current_app_pid{0};

inline void RequestSteamVRShutdownWithApp(uint32_t pid, int app_close_timeout_seconds = 30)
{
    if (pid == 0 || !IsProcessRunning(pid)) {
        RequestSteamVRShutdown();
        return;
    }
    // No WM_CLOSE equivalent — SIGTERM is the polite ask on Linux.
    kill((pid_t)pid, SIGTERM);
    const int poll_ms = 500;
    const int max_polls = (app_close_timeout_seconds * 1000) / poll_ms;
    for (int i = 0; i < max_polls; ++i) {
        if (!IsProcessRunning(pid)) {
            RequestSteamVRShutdown();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    LOG() << "auto_exit: pid " << pid << " still running after "
          << app_close_timeout_seconds << "s, terminating";
    kill((pid_t)pid, SIGKILL);
    RequestSteamVRShutdown();
}

//-----------------------------------------------------------------------------
// Focus helpers — not implementable on Wayland; X11 topmost is handled inside
// the X11 presenter. Stubs keep shared call sites simple.
//-----------------------------------------------------------------------------
inline void* GetHWNDFromPID(uint32_t) { return nullptr; }

//-----------------------------------------------------------------------------
// Processes that should never be profiled as "the game". Mirrors the Windows
// list (Proton games keep their .exe comm names) plus native SteamVR/Steam
// process names.
//-----------------------------------------------------------------------------
static const std::unordered_set<std::string> Skip_Processes = {
        // Native Linux SteamVR / Steam infrastructure
        "vrcompositor", "vrserver", "vrmonitor", "vrstartup", "vrcmd",
        "vrdashboard", "vrpathreg", "vrwebhelper", "vrserverhelper",
        "vrurlhandler", "steam", "steamwebhelper", "srt-bwrap",
        "pressure-vessel", "reaper", "pv-bwrap",
        // Windows names (Proton helpers etc.)
        "vrcompositor.exe", "vrserver.exe", "vrmonitor.exe", "vrstartup.exe",
        "removeusbhelper.exe", "restarthelper.exe", "vrcmd.exe",
        "vrdashboard.exe", "vrpathreg.exe", "vrwebhelper.exe",
        "vrprismhost.exe", "vrserverhelper.exe", "vrservice.exe",
        "vrurlhandler.exe", "steam.exe", "steamwebhelper.exe",
        "steamerrorreporter.exe", "steamservice.exe", "rundll32.exe",
        "rundll64.exe",
        // Third-party utilities / overlays (same as Windows list)
        "reviveoverlay.exe", "reviveinjector.exe", "fpsvr.exe", "driver4vr.exe",
        "psvr2_dialog.exe", "psvr2_overlay.exe",
        "lhrreceiverserver.exe", "rrconsole.exe", "htcconnectionutility.exe",
        "vivestreamingclient.exe", "vivestreamingserver.exe",
        "ovrserver_x64.exe", "ovrservicelauncher.exe", "ovrredir.exe",
        "oculusclient.exe", "oculusdash.exe",
        "pitool.exe", "piserver.exe", "piservicelauncher.exe", "pvrclient.exe",
        "varjobase.exe", "varjocompositor.exe", "varjosession.exe",
        "mixedrealityportal.exe", "holoshellapp.exe", "wmrgfxmonitor.exe",
        "alvr_dashboard.exe", "alvr.exe", "wivrn.exe", "wivrn-server",
        "virtualdesktop.streamer.exe", "virtualdesktop.service.exe",
        "streaming assistant.exe", "picovr streaming assistant.exe",
        "ps_server.exe", "custom-headset-gui.exe",
        "liv.exe", "xsoverlay.exe", "ovrtoolkit.exe", "oyasumi.exe",
        "ovradvancedsettings.exe", "advancedsettings.exe",
        "openvr-spacecalibrator.exe", "openvr-spacecalibratorex.exe",
        "bhapticsplayer.exe", "slimevr-server.exe", "vrcfacetracking.exe",
        "eyetrackvr.exe"
};

// Single-instance lock file (flock'd in device_provider on Linux).
static constexpr const char* kVRto3DLockName = "vrto3d-driver.lock";

//-----------------------------------------------------------------------------
// User-settings hotkey evaluation. The evaluator itself is platform-neutral
// (vrto3dlib/hotkey_eval.hpp, shared with win32_helper.hpp); this wrapper only
// supplies the Linux key-state query (evdev via input::IsKeyDown).
//-----------------------------------------------------------------------------
using vrto3d::DepthConvBackend;

inline std::string ApplyUserSettingsHotkeys(
    StereoDisplayDriverConfiguration& cfg,
    bool got_xinput,
    uint32_t xstate,
    const DepthConvBackend& b,
    float maxDelta = 0.001f)
{
    return vrto3d::ApplyUserSettingsHotkeysImpl(
        cfg, got_xinput, xstate, b,
        [](int vk) { return isDown(vk); }, maxDelta);
}
