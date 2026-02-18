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


// Configuration for VRto3D
struct StereoDisplayDriverConfiguration
{
    int32_t display_index;

    int32_t window_x;
    int32_t window_y;

    int32_t window_width;
    int32_t window_height;

    int32_t render_width;
    int32_t render_height;

    float hmd_height;
    float hmd_x;
    float hmd_y;
    float hmd_yaw;

    float aspect_ratio;
    float fov;
    float depth;
    float convergence;
    bool async_enable;
    bool disable_hotkeys;

    bool tab_enable;
    int32_t framepack_offset;
    bool reverse_enable;
    bool vd_fsbs_hack;
    bool dash_enable;
    bool auto_focus;

    float display_latency;
    float display_frequency;
    int32_t sleep_count_max;

    bool pitch_enable;
    bool yaw_enable;
    bool pitch_set;
    bool yaw_set;
    bool use_open_track;
    int32_t open_track_port;
    std::string launch_script;
    int32_t pose_reset_key;
    std::string pose_reset_str;
    bool reset_xinput;
    bool pose_reset;
    int32_t ctrl_toggle_key;
    std::string ctrl_toggle_str;
    bool ctrl_xinput;
    int32_t ctrl_type;
    std::string ctrl_type_str;
    bool ctrl_held;
    float pitch_radius;
    float ctrl_deadzone;
    float ctrl_sensitivity;

    size_t num_user_settings;
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
