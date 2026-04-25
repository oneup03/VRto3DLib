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

#include <string>
#include <vector>


// Output format selected for the built-in DX11 presenter (or alternate presenter).
// Compositor always renders canonical 2W x H SbS upstream; the presenter repacks.
enum class OutputMode : int {
    SbS = 0,               // 2W x H native side-by-side (or 2W x 2H with vd_fsbs_hack)
    SbSLeftFlip,           // SbS, but the left half is flipped vertically
    TaB,                   // W  x 2H top/bottom; framepack_offset inserts a gap
    RowInterlaced,         // W  x H, alternating rows (passive 3D TVs)
    ColInterlaced,         // W  x H, alternating columns
    Checkerboard,          // W  x H, (x+y)%2 eye selection
    AnaglyphRedCyan,
    AnaglyphGreenMagenta,
    LeiaSrWeaver,          // alternate: hand SRV to SR::IDX11Weaver1
    NvStereoDX9,           // alternate: 3D Vision via NVAPI + D3D9Ex (JSON value: "3DVisionDX9")
};

OutputMode OutputModeFromString(const std::string& s, OutputMode fallback = OutputMode::SbS);
std::string OutputModeToString(OutputMode m);


// Configuration for VRto3D
// Default values mirror default_config_ in json_manager.h — keep in sync.
struct StereoDisplayDriverConfiguration
{
    int32_t display_index    = 0;
    bool multi_display       = false;

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

    OutputMode output_mode   = OutputMode::SbS;
    int32_t framepack_offset = 0;
    bool eye_swap            = false;
    bool vd_fsbs_hack        = false;
    bool dash_enable         = false;
    bool auto_focus          = true;

    // Computed at driver activation from the target monitor (display_index).
    // Not read from JSON.
    float display_latency    = 0.011f;
    float display_frequency  = 60.0f;
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
    std::vector<int32_t> user_store_key;
    std::vector<std::string> user_store_str;
    std::vector<int32_t> user_key_type;
    std::vector<std::string> user_type_str;
    std::vector<float> user_depth;
    std::vector<float> user_convergence;
    std::vector<float> user_fov;
    std::vector<float> prev_depth;
    std::vector<float> prev_convergence;
    std::vector<float> prev_fov;
    std::vector<bool> was_held;
    std::vector<bool> load_xinput;
    std::vector<int32_t> sleep_count;
};
