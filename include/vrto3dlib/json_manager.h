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
#include <nlohmann/json.hpp>

#include "vrto3dlib/stereo_config.h"


const std::string DEF_CFG = "default_config.json";

class JsonManager {
public:
    JsonManager();

    void EnsureDefaultConfigExists();
    void LoadParamsFromJson(StereoDisplayDriverConfiguration& config);
    bool LoadProfileFromJson(const std::string& filename, StereoDisplayDriverConfiguration& config);
    void SaveProfileToJson(const std::string& filename, StereoDisplayDriverConfiguration& config);
    void SaveHmdOffsets(StereoDisplayDriverConfiguration& config);

private:
    
    // Create the example default JSON
    nlohmann::ordered_json default_config_ = {
        {"window_width", 1920},
        {"window_height", 1080},
        {"render_width", 1920},
        {"render_height", 1080},
        {"hmd_height", 1.0},
        {"hmd_x", 0.0},
        {"hmd_y", 0.0},
        {"hmd_yaw", 0.0},
        {"aspect_ratio", 1.77778},
        {"fov", 90.0},
        {"depth", 0.1},
        {"convergence", 1.0},
        {"async_enable", false},
        {"disable_hotkeys", false},
        {"tab_enable", false},
        {"framepack_offset", 0},
        {"reverse_enable", false},
        {"vd_fsbs_hack", false},
        {"dash_enable", false},
        {"auto_focus", true},
        {"display_latency", 0.011},
        {"display_frequency", 60.0},
        {"pitch_enable", false},
        {"yaw_enable", false},
        {"use_open_track", false},
        {"open_track_port", 4242},
        {"pose_reset_key", "VK_NUMPAD7"},
        {"ctrl_toggle_key", "VK_NUMPAD8"},
        {"ctrl_toggle_type", "toggle"},
        {"pitch_radius", 0.0},
        {"ctrl_deadzone", 0.05},
        {"ctrl_sensitivity", 1.0},
        {"user_settings", {
            {
                {"user_load_key", "VK_NUMPAD1"},
                {"user_store_key", "VK_NUMPAD4"},
                {"user_key_type", "switch"},
                {"user_depth", 0.1},
                {"user_convergence", 1.0}
            },
            {
                {"user_load_key", "XINPUT_GAMEPAD_GUIDE"},
                {"user_store_key", "VK_NUMPAD5"},
                {"user_key_type", "toggle"},
                {"user_depth", 0.065},
                {"user_convergence", 1.0}
            },
            {
                {"user_load_key", "VK_NUMPAD3"},
                {"user_store_key", "VK_NUMPAD6"},
                {"user_key_type", "hold"},
                {"user_depth", 0.065},
                {"user_convergence", 1.0}
            }
        }}
    };
    
    std::string vrto3dFolder;
    std::string getDocumentsFolderPath();
    void writeJsonToFile(const std::string& fileName, const nlohmann::ordered_json& jsonData);
    nlohmann::json readJsonFromFile(const std::string& fileName);
    nlohmann::ordered_json reorderFillJson(const nlohmann::json& target_json);
    void createFolderIfNotExist(const std::string& path);
    std::vector<std::string> split(const std::string& str, char delimiter);

    template <typename T>
    T getValue(const nlohmann::json& jsonConfig, const std::string& key);
};
