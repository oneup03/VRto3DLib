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

#include <cstdint>
#include <string>
#include <vector>


// Output format selected for the built-in DX11 presenter (or alternate presenter).
// Compositor always renders canonical 2W x H SbS upstream; the presenter repacks.
enum class OutputMode : int {
    SbS = 0,                       // 2W x H native side-by-side
    TaB,                           // W  x 2H top/bottom
    RowInterlaced,                 // W  x H, alternating rows (passive 3D TVs)
    ColInterlaced,                 // W  x H, alternating columns
    Checkerboard,                  // W  x H, (x+y)%2 eye selection
    LeiaSR,                        // alternate: SR Display Weaver
    NvidiaDX9,                     // alternate: 3D Vision via NVAPI + D3D9Ex
    WibbleWobble,                  // alternate: Frame Sequential via WibbleWobbleClient
    VirtualDesktop,                // Full-SbS 2W x H in a 2W x 2H window with black bars
    FramePacked720p60,             // HDMI 1.4 frame pack: 1280x1470 @60Hz (30px gap)
    FramePacked1080p24,            // HDMI 1.4 frame pack: 1920x2205 @24Hz (45px gap)
    FramePacked1080p60,            // HDMI 1.4 frame pack: 1920x2205 @60Hz (45px gap)
    FramePacked1080p60CVT,         // HDMI 1.4 frame pack: 1920x2205 @60Hz CVT blanking (45px gap)
    DualDisplay,                   // SbS spanning two contiguous identical monitors (left=mon1, right=mon2)
    DualDisplayFlip,               // DualDisplay, but the left image is flipped vertically
    AnaglyphRedCyan,               // simple R | GB split
    AnaglyphRedCyanDubois,         // Dubois optimized R/C
    AnaglyphRedCyanDeghosted,      // Deghosted R/C
    AnaglyphRedCyanCompromise,     // Compromise R/C
    AnaglyphGreenMagenta,          // simple G | RB split
    AnaglyphGreenMagentaDubois,    // Dubois optimized G/M
    AnaglyphGreenMagentaDeghosted, // Deghosted G/M
    AnaglyphBlueAmber,             // ColorCode-style B/A
};

OutputMode OutputModeFromString(const std::string& s, OutputMode fallback = OutputMode::SbS);
std::string OutputModeToString(OutputMode m);


// ---------------------------------------------------------------------------
// HDMI 1.4 frame-packing timing specifications
// ---------------------------------------------------------------------------
struct FramePackTimingSpec {
    uint32_t active_w;       // visible horizontal pixels (e.g. 1920)
    uint32_t active_h;       // visible vertical pixels, both eyes + gap (e.g. 2205)
    uint32_t per_eye_h;      // single-eye height (e.g. 1080)
    uint32_t gap_pixels;     // blanking gap between the two eyes (e.g. 45)
    float    refresh_hz;     // target refresh rate

    // Horizontal blanking
    uint16_t h_total;
    uint16_t h_front_porch;
    uint16_t h_sync_width;
    uint16_t h_back_porch;

    // Vertical blanking
    uint16_t v_total;
    uint16_t v_front_porch;
    uint16_t v_sync_width;
    uint16_t v_back_porch;
};

// Returns true if the given OutputMode is one of the FramePacked variants.
inline bool IsFramePackedMode(OutputMode m) {
    return m == OutputMode::FramePacked720p60
        || m == OutputMode::FramePacked1080p24
        || m == OutputMode::FramePacked1080p60
        || m == OutputMode::FramePacked1080p60CVT;
}

// Returns the FramePackTimingSpec for a frame-packed mode. Caller must check
// IsFramePackedMode() first — returns nullptr for non-frame-packed modes.
const FramePackTimingSpec* GetFramePackTimingSpec(OutputMode m);


// Configuration for VRto3D
// Default values mirror default_config_ in json_manager.h — keep in sync.
struct StereoDisplayDriverConfiguration
{
    int32_t display_index    = 0;
    OutputMode output_mode   = OutputMode::SbS;
    bool eye_swap            = false;

    int32_t window_x         = 0;
    int32_t window_y         = 0;

    int32_t window_width     = 0;
    int32_t window_height    = 0;

    int32_t render_width     = 1920;
    int32_t render_height    = 1080;

    float hmd_height         = 1.0f;
    float hmd_x              = 0.0f;
    float hmd_y              = 0.0f;
    float hmd_yaw            = 0.0f;

    float aspect_ratio       = 1.77778f;
    float fov                = 90.0f;
    float depth              = 0.1f;
    float convergence        = 1.0f;
    bool async_enable        = false;
    bool disable_hotkeys     = false;

    // Auto-depth: GPU disparity analysis on the SbS frame drives a runtime
    // floor on `depth` so the closest object's on-screen disparity stays under
    // `auto_depth_target_disparity` (fraction of one eye's width). When toggled
    // off, depth snaps back to the user's manual ceiling (StereoDisplayComponent
    // tracks this as `manual_depth_`).
    bool  auto_depth_enabled         = false;
    float auto_depth_target_disparity = 0.005f;
    float auto_depth_smoothing       = 0.08f;

    bool dash_enable         = false;
    bool auto_focus          = true;

    // Computed at driver activation from the target monitor (display_index).
    // Not read from JSON.
    float display_latency    = 0.011f;
    // 0 = auto-detect from the target monitor at driver activation; any
    // non-zero value in the JSON config overrides the auto-detection.
    float display_frequency  = 0.0f;
    int32_t sleep_count_max  = 0;

    bool pitch_enable        = false;
    bool yaw_enable          = false;
    bool pitch_set           = false;
    bool yaw_set             = false;
    bool use_open_track      = false;
    int32_t open_track_port  = 4242;
    bool use_track_filter    = false;
    float trk_flt_rot_sens   = 0.5f;
    float trk_flt_pos_sens   = 0.25f;
    float trk_flt_rot_dz     = 0.03f;
    float trk_flt_pos_dz     = 0.02f;
    float trk_flt_zoom_smooth = 0.0f;
    float trk_flt_max_zoom   = 10.0f;

    // LeiaSR built-in head tracking (LeiaSrPresenter -> One-Euro filter ->
    // OpenTrack UDP -> open_track_port). Active only when output_mode==LeiaSR
    // AND use_open_track==true. Defaults mirror Simulated-Reality-OpenTrack-Bridge.
    float sr_filter_pos_mincutoff = 0.08f;
    float sr_filter_pos_beta      = 0.08f;
    float sr_filter_rot_mincutoff = 0.12f;
    float sr_filter_rot_beta      = 0.01f;
    float sr_angle_deadzone_deg   = 0.2f;
    float sr_sens_yaw             = 1.0f;
    float sr_sens_pitch           = 1.0f;
    float sr_sens_roll            = 1.0f;
    float sr_max_yaw              = 70.0f;
    float sr_max_pitch            = 70.0f;
    float sr_max_roll             = 70.0f;
    // One of: "XYZ_YawPitch" (default), "XYZ", "YawPitch", "Full6DOF", "YawPitchRoll"
    std::string sr_track_mode     = "XYZ_YawPitch";
    std::string launch_script;
    int32_t pose_reset_key   = 0;
    std::string pose_reset_str = "VK_NUMPAD7";
    bool reset_xinput        = false;
    bool pose_reset          = false;
    int32_t ctrl_toggle_key  = 0;
    std::string ctrl_toggle_str = "VK_NUMPAD8";
    bool ctrl_xinput         = false;
    int32_t ctrl_type        = 0;
    std::string ctrl_type_str = "toggle";
    bool ctrl_held           = false;
    float pitch_radius       = 0.0f;
    float ctrl_deadzone      = 0.05f;
    float ctrl_sensitivity   = 1.0f;

    size_t num_user_settings = 0;
    std::vector<int32_t> user_load_key;
    std::vector<std::string> user_load_str;
    std::vector<int32_t> user_key_type;
    std::vector<std::string> user_type_str;
    // Per-row preset lists. Each element is a list of presets the user can
    // cycle through with a single hotkey (comma-separated in the OSD menu).
    // Length 1 (== legacy single-preset) is the common case.
    std::vector<std::vector<float>> user_depth;
    std::vector<std::vector<float>> user_convergence;
    std::vector<std::vector<float>> user_fov;
    // Current cycle index into user_depth[i] / user_convergence[i] / user_fov[i].
    // Advanced on each load-key press (SWITCH/TOGGLE modes). HOLD always uses
    // index 0.
    std::vector<size_t> user_preset_index;
    // "Previous" snapshots for HOLD/TOGGLE revert-on-release / revert-on-match.
    std::vector<float> prev_depth;
    std::vector<float> prev_convergence;
    std::vector<float> prev_fov;
    std::vector<bool> was_held;
    std::vector<bool> load_xinput;
    std::vector<int32_t> sleep_count;
};
