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

#define WIN32_LEAN_AND_MEAN
#include "vrto3dlib/json_manager.h"
#include "vrto3dlib/debug_log.hpp"
#include "vrto3dlib/key_mappings.h"
#include "vrto3dlib/win32_helper.hpp"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <iomanip>
#include <sstream>

// Include the nlohmann/json library
#include <nlohmann/json.hpp>


//-----------------------------------------------------------------------------
// OutputMode string helpers (declared in stereo_config.h)
//-----------------------------------------------------------------------------
OutputMode OutputModeFromString(const std::string& s, OutputMode fallback)
{
    if (s == "SbS")                            return OutputMode::SbS;
    if (s == "TaB")                            return OutputMode::TaB;
    if (s == "RowInterlaced")                  return OutputMode::RowInterlaced;
    if (s == "ColInterlaced")                  return OutputMode::ColInterlaced;
    if (s == "Checkerboard")                   return OutputMode::Checkerboard;
    if (s == "LeiaSR")                         return OutputMode::LeiaSR;
    if (s == "NvidiaDX9")                      return OutputMode::NvidiaDX9;
    if (s == "WibbleWobble")                   return OutputMode::WibbleWobble;
    if (s == "VirtualDesktop")                 return OutputMode::VirtualDesktop;
    if (s == "FramePacked720p60")              return OutputMode::FramePacked720p60;
    if (s == "FramePacked1080p24")             return OutputMode::FramePacked1080p24;
    if (s == "FramePacked1080p60")             return OutputMode::FramePacked1080p60;
    if (s == "FramePacked1080p60CVT")          return OutputMode::FramePacked1080p60CVT;
    if (s == "DualDisplay")                    return OutputMode::DualDisplay;
    if (s == "DualDisplayFlip")                return OutputMode::DualDisplayFlip;
    if (s == "AnaglyphRedCyan")                return OutputMode::AnaglyphRedCyan;
    if (s == "AnaglyphRedCyanDubois")          return OutputMode::AnaglyphRedCyanDubois;
    if (s == "AnaglyphRedCyanDeghosted")       return OutputMode::AnaglyphRedCyanDeghosted;
    if (s == "AnaglyphRedCyanCompromise")      return OutputMode::AnaglyphRedCyanCompromise;
    if (s == "AnaglyphGreenMagenta")           return OutputMode::AnaglyphGreenMagenta;
    if (s == "AnaglyphGreenMagentaDubois")     return OutputMode::AnaglyphGreenMagentaDubois;
    if (s == "AnaglyphGreenMagentaDeghosted")  return OutputMode::AnaglyphGreenMagentaDeghosted;
    if (s == "AnaglyphBlueAmber")              return OutputMode::AnaglyphBlueAmber;
    if (s == "Mono")                           return OutputMode::Mono;
    return fallback;
}

std::string OutputModeToString(OutputMode m)
{
    switch (m) {
        case OutputMode::SbS:                            return "SbS";
        case OutputMode::TaB:                            return "TaB";
        case OutputMode::RowInterlaced:                  return "RowInterlaced";
        case OutputMode::ColInterlaced:                  return "ColInterlaced";
        case OutputMode::Checkerboard:                   return "Checkerboard";
        case OutputMode::LeiaSR:                         return "LeiaSR";
        case OutputMode::NvidiaDX9:                      return "NvidiaDX9";
        case OutputMode::WibbleWobble:                   return "WibbleWobble";
        case OutputMode::VirtualDesktop:                 return "VirtualDesktop";
        case OutputMode::FramePacked720p60:              return "FramePacked720p60";
        case OutputMode::FramePacked1080p24:             return "FramePacked1080p24";
        case OutputMode::FramePacked1080p60:             return "FramePacked1080p60";
        case OutputMode::FramePacked1080p60CVT:          return "FramePacked1080p60CVT";
        case OutputMode::DualDisplay:                    return "DualDisplay";
        case OutputMode::DualDisplayFlip:                return "DualDisplayFlip";
        case OutputMode::AnaglyphRedCyan:                return "AnaglyphRedCyan";
        case OutputMode::AnaglyphRedCyanDubois:          return "AnaglyphRedCyanDubois";
        case OutputMode::AnaglyphRedCyanDeghosted:       return "AnaglyphRedCyanDeghosted";
        case OutputMode::AnaglyphRedCyanCompromise:      return "AnaglyphRedCyanCompromise";
        case OutputMode::AnaglyphGreenMagenta:           return "AnaglyphGreenMagenta";
        case OutputMode::AnaglyphGreenMagentaDubois:     return "AnaglyphGreenMagentaDubois";
        case OutputMode::AnaglyphGreenMagentaDeghosted:  return "AnaglyphGreenMagentaDeghosted";
        case OutputMode::AnaglyphBlueAmber:              return "AnaglyphBlueAmber";
        case OutputMode::Mono:                           return "Mono";
    }
    return "SbS";
}


//-----------------------------------------------------------------------------
// HDMI 1.4 frame-packing timing specs (declared in stereo_config.h)
//-----------------------------------------------------------------------------
static const FramePackTimingSpec s_frame_pack_timings[] = {
    // FramePacked720p60:  1280x1470 @60Hz, 30px gap
    //   H: 1650 total = 1280 active + 110 front + 40 sync + 220 back
    //   V: 1500 total = 1470 active +   5 front +  5 sync +  20 back
    //   Pixel clock: 1650 * 1500 * 60 = 148.5 MHz (HDMI 1.4 standard)
    { 1280, 1470, 720, 30, 60.0f,   1650, 110, 40, 220,   1500, 5, 5, 20 },

    // FramePacked1080p24: 1920x2205 @24Hz, 45px gap
    //   H: 2750 total = 1920 active + 638 front + 44 sync + 148 back
    //   V: 2250 total = 2205 active +   4 front +  5 sync +  36 back
    //   Pixel clock: 2750 * 2250 * 24 = 148.5 MHz (HDMI 1.4 standard)
    { 1920, 2205, 1080, 45, 24.0f,  2750, 638, 44, 148,   2250, 4, 5, 36 },

    // FramePacked1080p60: 1920x2205 @60Hz, 45px gap
    //   H: 2750 total = 1920 active + 638 front + 44 sync + 148 back
    //   V: 2250 total = 2205 active +   4 front +  5 sync +  36 back
    //   Pixel clock: 2750 * 2250 * 60 = 371.25 MHz (requires HDMI 2.0+)
    { 1920, 2205, 1080, 45, 60.0f,  2750, 638, 44, 148,   2250, 4, 5, 36 },

    // FramePacked1080p60CVT: 1920x2205 @60Hz CVT blanking, 45px gap
    //   H: 2080 total = 1920 active + 48 front + 32 sync + 80 back
    //   V: 2250 total = 2205 active +  4 front +  5 sync + 36 back
    //   Pixel clock: 2080 * 2250 * 60 = 280.8 MHz (fits most HDMI 2.0 displays)
    { 1920, 2205, 1080, 45, 60.0f,  2080, 48, 32, 80,     2250, 4, 5, 36 },
};

const FramePackTimingSpec* GetFramePackTimingSpec(OutputMode m)
{
    switch (m) {
        case OutputMode::FramePacked720p60:     return &s_frame_pack_timings[0];
        case OutputMode::FramePacked1080p24:    return &s_frame_pack_timings[1];
        case OutputMode::FramePacked1080p60:    return &s_frame_pack_timings[2];
        case OutputMode::FramePacked1080p60CVT: return &s_frame_pack_timings[3];
        default:                                return nullptr;
    }
}


JsonManager::JsonManager() {
    vrto3dFolder = GetSteamInstallPath();
    if (vrto3dFolder != "")
    {
        vrto3dFolder += "\\config\\vrto3d";
        createFolderIfNotExist(vrto3dFolder);
    }
}


//-----------------------------------------------------------------------------
// Purpose: Create vrto3d folder if it doesn't exist
//-----------------------------------------------------------------------------
void JsonManager::createFolderIfNotExist(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}


//-----------------------------------------------------------------------------
// Purpose: Write a JSON to Steam/config/vrto3d
//-----------------------------------------------------------------------------
void JsonManager::writeJsonToFile(const std::string& fileName, const nlohmann::ordered_json& jsonData) {
    std::string filePath = vrto3dFolder + "\\" + fileName;
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << jsonData.dump(4); // Pretty-print the JSON with an indent of 4 spaces
        file.close();
        LOG() << "Saved profile: " << fileName;
    }
    else {
        LOG() << "Failed to save profile: " << fileName;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Read a JSON from Steam/config/vrto3d. Returns an empty json on
//          missing file, empty file, or parse error — callers must handle the
//          "no data" case (LoadParamsFromJson regenerates defaults; the
//          per-profile Load returns false).
//-----------------------------------------------------------------------------
nlohmann::json JsonManager::readJsonFromFile(const std::string& fileName) {
    std::string filePath = vrto3dFolder + "\\" + fileName;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return {};
    }

    // Empty-file guard: nlohmann's stream operator throws on EOF before any
    // token. Detect explicitly so we don't generate a spurious parse error.
    file.seekg(0, std::ios::end);
    std::streampos size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
        LOG() << "readJsonFromFile: " << fileName << " is empty; ignoring";
        return {};
    }

    nlohmann::json jsonData;
    try {
        file >> jsonData;
    } catch (const nlohmann::json::parse_error& e) {
        LOG() << "readJsonFromFile: " << fileName << " is corrupt ("
              << e.what() << "); falling back to defaults";
        return {};
    } catch (const std::exception& e) {
        LOG() << "readJsonFromFile: " << fileName << " unexpected error ("
              << e.what() << "); ignoring";
        return {};
    }
    return jsonData;
}


//-----------------------------------------------------------------------------
// Purpose: Ensure default_config has all valid settings present
//-----------------------------------------------------------------------------
nlohmann::ordered_json JsonManager::reorderFillJson(const nlohmann::json& target_json)
{
    nlohmann::ordered_json result;
    for (const auto& [key, source_value] : default_config_.items()) {
        if (key == "user_settings" && target_json.contains(key)) {
            result[key] = target_json.at(key);
        }
        else if (target_json.contains(key)) {
            result[key] = target_json.at(key);
        }
        else {
            result[key] = source_value;
        }
    }
    return result;
}


//-----------------------------------------------------------------------------
// Purpose: Split a string by a delimiter
//-----------------------------------------------------------------------------
std::vector<std::string> JsonManager::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}


//-----------------------------------------------------------------------------
// Purpose: Create default_config.json if it doesn't exist
//-----------------------------------------------------------------------------
void JsonManager::EnsureDefaultConfigExists()
{
    auto writeDefaults = [&](const char* reason) {
        std::string filePath = vrto3dFolder + "\\" + DEF_CFG;
        LOG() << DEF_CFG << ": " << reason << " — writing fresh defaults";
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << default_config_.dump(4);
            file.close();
            LOG() << "Default config written to " << DEF_CFG;
        } else {
            LOG() << "Failed to open " << DEF_CFG << " for writing";
        }
    };

    std::string filePath = vrto3dFolder + "\\" + DEF_CFG;
    if (!std::filesystem::exists(filePath)) {
        writeDefaults("does not exist");
        return;
    }

    LOG() << "Default config already exists. Checking for missing/default keys...";
    nlohmann::json existing_json = readJsonFromFile(DEF_CFG);

    // readJsonFromFile already swallows parse errors and returns {}. If the
    // file was empty/corrupt or otherwise unusable as an object, regenerate.
    if (existing_json.is_null() || !existing_json.is_object() || existing_json.empty()) {
        writeDefaults("empty or corrupt");
        return;
    }

    nlohmann::ordered_json merged_json = reorderFillJson(existing_json);
    writeJsonToFile(DEF_CFG, merged_json);
    LOG() << "Updated config written with defaults filled in";
}


//-----------------------------------------------------------------------------
// Purpose: Get a value from jsonConfig or fallback to defaultConfig
//-----------------------------------------------------------------------------
template <typename T>
T JsonManager::getValue(const nlohmann::json& jsonConfig, const std::string& key) {
    if (jsonConfig.contains(key)) {
        try {
            return jsonConfig[key].get<T>();
        } catch (const nlohmann::json::exception& e) {
            LOG() << "getValue: key \"" << key << "\" wrong type ("
                  << e.what() << "); using default";
        }
    }
    return default_config_[key].get<T>();
}


//-----------------------------------------------------------------------------
// Purpose: Load the VRto3D display from a JSON file
//-----------------------------------------------------------------------------
void JsonManager::LoadParamsFromJson(StereoDisplayDriverConfiguration& config)
{
    try {
        // Read the JSON configuration from the file
        nlohmann::json jsonConfig = readJsonFromFile(DEF_CFG);

        // If the file was missing/empty/corrupt, readJsonFromFile returned {}.
        // Recreate it from the in-memory defaults so subsequent loads, the
        // OSD menu, and any future SaveFullConfigToJson all see a coherent
        // baseline on disk.
        if (jsonConfig.is_null() || !jsonConfig.is_object() || jsonConfig.empty()) {
            LOG() << "LoadParamsFromJson: " << DEF_CFG
                  << " missing/empty/corrupt — regenerating from defaults";
            std::string filePath = vrto3dFolder + "\\" + DEF_CFG;
            std::ofstream file(filePath);
            if (file.is_open()) {
                file << default_config_.dump(4);
                file.close();
            }
            jsonConfig = default_config_;  // continue with in-memory defaults
        }

        // Load values directly from the base level of the JSON
        config.display_index = getValue<int>(jsonConfig, "display_index");
        config.output_mode = OutputModeFromString(getValue<std::string>(jsonConfig, "output_mode"));
        config.eye_swap = getValue<bool>(jsonConfig, "eye_swap");
        config.render_width = getValue<int>(jsonConfig, "render_width");
        config.render_height = getValue<int>(jsonConfig, "render_height");
        config.display_frequency = getValue<float>(jsonConfig, "display_frequency");

        config.hmd_x = getValue<float>(jsonConfig, "hmd_x");
        config.hmd_y = getValue<float>(jsonConfig, "hmd_y");
        config.hmd_yaw = getValue<float>(jsonConfig, "hmd_yaw");

        config.async_enable = getValue<bool>(jsonConfig, "async_enable");
        config.disable_hotkeys = getValue<bool>(jsonConfig, "disable_hotkeys");
        config.dash_enable = getValue<bool>(jsonConfig, "dash_enable");
        config.auto_focus = getValue<bool>(jsonConfig, "auto_focus");
        config.use_open_track = getValue<bool>(jsonConfig, "use_open_track");
        config.open_track_port = getValue<int>(jsonConfig, "open_track_port");
        config.use_track_filter = getValue<bool>(jsonConfig, "use_track_filter");
        config.trk_flt_rot_sens = getValue<float>(jsonConfig, "trk_flt_rot_sens");
        config.trk_flt_pos_sens = getValue<float>(jsonConfig, "trk_flt_pos_sens");
        config.trk_flt_rot_dz = getValue<float>(jsonConfig, "trk_flt_rot_dz");
        config.trk_flt_pos_dz = getValue<float>(jsonConfig, "trk_flt_pos_dz");
        config.trk_flt_zoom_smooth = getValue<float>(jsonConfig, "trk_flt_zoom_smooth");
        config.trk_flt_max_zoom = getValue<float>(jsonConfig, "trk_flt_max_zoom");
        config.sr_filter_pos_mincutoff = getValue<float>(jsonConfig, "sr_filter_pos_mincutoff");
        config.sr_filter_pos_beta      = getValue<float>(jsonConfig, "sr_filter_pos_beta");
        config.sr_filter_rot_mincutoff = getValue<float>(jsonConfig, "sr_filter_rot_mincutoff");
        config.sr_filter_rot_beta      = getValue<float>(jsonConfig, "sr_filter_rot_beta");
        config.sr_angle_deadzone_deg   = getValue<float>(jsonConfig, "sr_angle_deadzone_deg");
        config.sr_sens_yaw             = getValue<float>(jsonConfig, "sr_sens_yaw");
        config.sr_sens_pitch           = getValue<float>(jsonConfig, "sr_sens_pitch");
        config.sr_sens_roll            = getValue<float>(jsonConfig, "sr_sens_roll");
        config.sr_max_yaw              = getValue<float>(jsonConfig, "sr_max_yaw");
        config.sr_max_pitch            = getValue<float>(jsonConfig, "sr_max_pitch");
        config.sr_max_roll             = getValue<float>(jsonConfig, "sr_max_roll");
        config.sr_track_mode           = getValue<std::string>(jsonConfig, "sr_track_mode");

        // LeiaSR + OpenTrack: force the consumer-side AHRS filter on. The SR
        // pipeline already smooths upstream, but the receiver expects filtered
        // input for stable pose composition. Also persist the override to
        // default_config.json so the on-disk value matches runtime behavior.
        if (config.output_mode == OutputMode::LeiaSR && config.use_open_track && !config.use_track_filter) {
            config.use_track_filter = true;
            LOG() << "LoadParamsFromJson: forcing use_track_filter=true (LeiaSR + use_open_track)";

            nlohmann::ordered_json merged = reorderFillJson(jsonConfig);
            merged["use_track_filter"] = true;
            writeJsonToFile(DEF_CFG, merged);
        }
        config.launch_script = getValue<std::string>(jsonConfig, "launch_script");

        config.sleep_count_max = (int)(floor(1600.0 / (1000.0 / config.display_frequency)));
    }
    catch (const nlohmann::json::exception& e) {
        LOG() << "Error reading default_config.json: " << e.what();
    }
}


//-----------------------------------------------------------------------------
// Purpose: Load a VRto3D profile from a JSON file
//-----------------------------------------------------------------------------
bool JsonManager::LoadProfileFromJson(const std::string& filename, StereoDisplayDriverConfiguration& config)
{
    try {
        // Read the JSON configuration from the file
        nlohmann::json jsonConfig = readJsonFromFile(filename);

        // readJsonFromFile returns {} for missing, empty, or corrupt files.
        // For game profiles that's a "no profile" signal; for default_config
        // we fall through with an empty json so getValue<>() pulls every key
        // from default_config_.
        if ((jsonConfig.is_null() || !jsonConfig.is_object() || jsonConfig.empty())
            && filename != DEF_CFG) {
            LOG() << "No profile (or unreadable) for " << filename;
            return false;
        }
        if (jsonConfig.is_null() || !jsonConfig.is_object() || jsonConfig.empty()) {
            LOG() << "LoadProfileFromJson: " << filename
                  << " missing/empty/corrupt — using in-memory defaults";
            jsonConfig = default_config_;
        }

        // Profile settings
        config.hmd_height = getValue<float>(jsonConfig, "hmd_height");
        config.aspect_ratio = getValue<float>(jsonConfig, "aspect_ratio");
        config.fov = getValue<float>(jsonConfig, "fov");
        config.depth = getValue<float>(jsonConfig, "depth");
        config.convergence = getValue<float>(jsonConfig, "convergence");
        if (jsonConfig.contains("async_enable")) {
            config.async_enable = getValue<bool>(jsonConfig, "async_enable");
        }
        if (jsonConfig.contains("auto_depth_enabled")) {
            config.auto_depth_enabled = getValue<bool>(jsonConfig, "auto_depth_enabled");
        }
        if (jsonConfig.contains("auto_depth_target_disparity")) {
            config.auto_depth_target_disparity = getValue<float>(jsonConfig, "auto_depth_target_disparity");
        }
        if (jsonConfig.contains("auto_depth_smoothing")) {
            config.auto_depth_smoothing = getValue<float>(jsonConfig, "auto_depth_smoothing");
        }

        // Controller settings
        config.pitch_enable = getValue<bool>(jsonConfig, "pitch_enable");
        config.pitch_set = config.pitch_enable;
        config.yaw_enable = getValue<bool>(jsonConfig, "yaw_enable");
        config.yaw_set = config.yaw_enable;

        config.pose_reset_str = getValue<std::string>(jsonConfig, "pose_reset_key");
        
        if (VirtualKeyMappings.find(config.pose_reset_str) != VirtualKeyMappings.end()) {
            config.pose_reset_key = VirtualKeyMappings[config.pose_reset_str];
            config.reset_xinput = false;
        }
        else if (XInputMappings.find(config.pose_reset_str) != XInputMappings.end() || config.pose_reset_str.find('+') != std::string::npos) {
            config.pose_reset_key = 0x0;
            auto hotkeys = split(config.pose_reset_str, '+');
            for (const auto& hotkey : hotkeys) {
                if (XInputMappings.find(hotkey) != XInputMappings.end()) {
                    config.pose_reset_key |= XInputMappings[hotkey];
                }
            }
            config.reset_xinput = true;
        }
        config.pose_reset = true;

        config.ctrl_toggle_str = getValue<std::string>(jsonConfig, "ctrl_toggle_key");
        if (VirtualKeyMappings.find(config.ctrl_toggle_str) != VirtualKeyMappings.end()) {
            config.ctrl_toggle_key = VirtualKeyMappings[config.ctrl_toggle_str];
            config.ctrl_xinput = false;
        }
        else if (XInputMappings.find(config.ctrl_toggle_str) != XInputMappings.end() || config.ctrl_toggle_str.find('+') != std::string::npos) {
            config.ctrl_toggle_key = 0x0;
            auto hotkeys = split(config.ctrl_toggle_str, '+');
            for (const auto& hotkey : hotkeys) {
                if (XInputMappings.find(hotkey) != XInputMappings.end()) {
                    config.ctrl_toggle_key |= XInputMappings[hotkey];
                }
            }
            config.ctrl_xinput = true;
        }

        config.ctrl_type_str = getValue<std::string>(jsonConfig, "ctrl_toggle_type");
        config.ctrl_type = KeyBindTypes[config.ctrl_type_str];

        config.pitch_radius = getValue<float>(jsonConfig, "pitch_radius");
        config.ctrl_deadzone = getValue<float>(jsonConfig, "ctrl_deadzone");
        config.ctrl_sensitivity = getValue<float>(jsonConfig, "ctrl_sensitivity");

        // Read user binds from user_settings array, falling back to defaults if missing or empty
        nlohmann::json user_settings_array;
        if (jsonConfig.contains("user_settings") && jsonConfig.at("user_settings").size() > 0)
            user_settings_array = jsonConfig.at("user_settings");
        else
            user_settings_array = default_config_.at("user_settings");

        // Resize vectors based on the size of the user_settings array
        config.num_user_settings = user_settings_array.size();
        config.user_load_key.resize(config.num_user_settings);
        config.user_key_type.resize(config.num_user_settings);
        config.user_depth.assign(config.num_user_settings, std::vector<float>{});
        config.user_convergence.assign(config.num_user_settings, std::vector<float>{});
        config.user_fov.assign(config.num_user_settings, std::vector<float>{});
        config.user_preset_index.assign(config.num_user_settings, 0);
        config.prev_depth.resize(config.num_user_settings);
        config.prev_convergence.resize(config.num_user_settings);
        config.prev_fov.resize(config.num_user_settings);
        config.was_held.resize(config.num_user_settings);
        config.load_xinput.resize(config.num_user_settings);
        config.sleep_count.resize(config.num_user_settings);
        config.user_load_str.resize(config.num_user_settings);
        config.user_type_str.resize(config.num_user_settings);

        for (size_t i = 0; i < config.num_user_settings; ++i) {
            const auto& user_setting = user_settings_array.at(i);

            config.user_load_str[i] = user_setting.at("user_load_key").get<std::string>();
            if (VirtualKeyMappings.find(config.user_load_str[i]) != VirtualKeyMappings.end()) {
                config.user_load_key[i] = VirtualKeyMappings[config.user_load_str[i]];
                config.load_xinput[i] = false;
            }
            else if (XInputMappings.find(config.user_load_str[i]) != XInputMappings.end() || config.user_load_str[i].find('+') != std::string::npos) {
                config.user_load_key[i] = 0x0;
                auto hotkeys = split(config.user_load_str[i], '+');
                for (const auto& hotkey : hotkeys) {
                    if (XInputMappings.find(hotkey) != XInputMappings.end()) {
                        config.user_load_key[i] |= XInputMappings[hotkey];
                    }
                }
                config.load_xinput[i] = true;
            }

            config.user_type_str[i] = user_setting.at("user_key_type").get<std::string>();
            if (KeyBindTypes.find(config.user_type_str[i]) != KeyBindTypes.end()) {
                config.user_key_type[i] = KeyBindTypes[config.user_type_str[i]];
            }

            // Each preset value can be a scalar (legacy) or an array (cycle).
            auto readNums = [](const nlohmann::json& v) {
                std::vector<float> out;
                if (v.is_number()) out.push_back(v.get<float>());
                else if (v.is_array()) for (auto& e : v) if (e.is_number()) out.push_back(e.get<float>());
                return out;
            };
            config.user_depth[i]       = readNums(user_setting.at("user_depth"));
            config.user_convergence[i] = readNums(user_setting.at("user_convergence"));
            if (user_setting.contains("user_fov")) {
                config.user_fov[i] = readNums(user_setting["user_fov"]);
            }
            // Pad missing fov to match depth/conv length, defaulting to global fov.
            const size_t n = (std::max)(config.user_depth[i].size(),
                                        config.user_convergence[i].size());
            if (config.user_depth[i].empty())       config.user_depth[i].push_back(0.0f);
            if (config.user_convergence[i].empty()) config.user_convergence[i].push_back(1.0f);
            while (config.user_depth[i].size()       < n) config.user_depth[i].push_back(config.user_depth[i].back());
            while (config.user_convergence[i].size() < n) config.user_convergence[i].push_back(config.user_convergence[i].back());
            while (config.user_fov[i].size()         < n) config.user_fov[i].push_back(config.fov);
            config.user_preset_index[i] = 0;
        }

    }
    catch (const nlohmann::json::exception& e) {
        LOG() << "Error reading config from " << filename.c_str() << ": " << e.what();
        return false;
    }

    return true;
}


//-----------------------------------------------------------------------------
// Purpose: Save Game Specific Settings to Steam\config\vrto3d\app_name_config.json
//-----------------------------------------------------------------------------
void JsonManager::SaveProfileToJson(const std::string& filename, StereoDisplayDriverConfiguration& config)
{
    // Create a JSON object to hold all the configuration data
    nlohmann::ordered_json jsonConfig;

    // Populate the JSON object with settings
    jsonConfig["hmd_height"] = config.hmd_height;
    jsonConfig["fov"] = config.fov;
    jsonConfig["depth"] = config.depth;
    jsonConfig["convergence"] = config.convergence;
    jsonConfig["async_enable"] = config.async_enable;
    jsonConfig["auto_depth_enabled"] = config.auto_depth_enabled;
    jsonConfig["auto_depth_target_disparity"] = config.auto_depth_target_disparity;
    jsonConfig["auto_depth_smoothing"] = config.auto_depth_smoothing;
    jsonConfig["pitch_enable"] = config.pitch_enable;
    jsonConfig["yaw_enable"] = config.yaw_enable;
    jsonConfig["pose_reset_key"] = config.pose_reset_str;
    jsonConfig["ctrl_toggle_key"] = config.ctrl_toggle_str;
    jsonConfig["ctrl_toggle_type"] = config.ctrl_type_str;
    jsonConfig["pitch_radius"] = config.pitch_radius;
    jsonConfig["ctrl_deadzone"] = config.ctrl_deadzone;
    jsonConfig["ctrl_sensitivity"] = config.ctrl_sensitivity;

    // Store user settings as an array, falling back to defaults if none are set.
    // Each preset value is written as a scalar when only one entry exists
    // (preserves legacy file format) or as an array for multi-preset cycles.
    auto writePreset = [](nlohmann::ordered_json& obj, const char* key,
                          const std::vector<float>& vals) {
        if (vals.size() == 1) obj[key] = vals[0];
        else                  obj[key] = vals;
    };
    if (config.num_user_settings > 0) {
        for (size_t i = 0; i < config.num_user_settings; i++) {
            nlohmann::ordered_json userSettings;
            userSettings["user_load_key"]   = config.user_load_str[i];
            userSettings["user_key_type"]   = config.user_type_str[i];
            writePreset(userSettings, "user_depth",       config.user_depth[i]);
            writePreset(userSettings, "user_convergence", config.user_convergence[i]);
            writePreset(userSettings, "user_fov",         config.user_fov[i]);
            jsonConfig["user_settings"].push_back(userSettings);
        }
    }
    else {
        jsonConfig["user_settings"] = default_config_.at("user_settings");
    }

    writeJsonToFile(filename, jsonConfig);
}


//-----------------------------------------------------------------------------
// Purpose: Save the FULL configuration (all driver-wide + per-profile keys)
//          to a JSON file. Mirrors the union of keys read by
//          LoadParamsFromJson + LoadProfileFromJson so saved output round-trips
//          cleanly. Use this for "Save default_config.json".
//-----------------------------------------------------------------------------
void JsonManager::SaveFullConfigToJson(const std::string& filename, StereoDisplayDriverConfiguration& config)
{
    nlohmann::ordered_json j;

    // Display / output
    j["display_index"]   = config.display_index;
    j["output_mode"]     = OutputModeToString(config.output_mode);
    j["eye_swap"]        = config.eye_swap;
    j["render_width"]    = config.render_width;
    j["render_height"]   = config.render_height;
    j["display_frequency"] = config.display_frequency;

    // HMD pose
    j["hmd_height"]      = config.hmd_height;
    j["hmd_x"]           = config.hmd_x;
    j["hmd_y"]           = config.hmd_y;
    j["hmd_yaw"]         = config.hmd_yaw;

    // Stereo
    j["aspect_ratio"]    = config.aspect_ratio;
    j["fov"]             = config.fov;
    j["depth"]           = config.depth;
    j["convergence"]     = config.convergence;
    j["async_enable"]    = config.async_enable;
    j["auto_depth_enabled"]          = config.auto_depth_enabled;
    j["auto_depth_target_disparity"] = config.auto_depth_target_disparity;
    j["auto_depth_smoothing"]        = config.auto_depth_smoothing;

    // Misc / system
    j["disable_hotkeys"] = config.disable_hotkeys;
    j["dash_enable"]     = config.dash_enable;
    j["auto_focus"]      = config.auto_focus;

    // Controller / tracking inputs
    j["pitch_enable"]    = config.pitch_enable;
    j["yaw_enable"]      = config.yaw_enable;
    j["use_open_track"]  = config.use_open_track;
    j["open_track_port"] = config.open_track_port;

    // Track filter
    j["use_track_filter"]    = config.use_track_filter;
    j["trk_flt_rot_sens"]    = config.trk_flt_rot_sens;
    j["trk_flt_pos_sens"]    = config.trk_flt_pos_sens;
    j["trk_flt_rot_dz"]      = config.trk_flt_rot_dz;
    j["trk_flt_pos_dz"]      = config.trk_flt_pos_dz;
    j["trk_flt_zoom_smooth"] = config.trk_flt_zoom_smooth;
    j["trk_flt_max_zoom"]    = config.trk_flt_max_zoom;

    // LeiaSR head tracking
    j["sr_filter_pos_mincutoff"] = config.sr_filter_pos_mincutoff;
    j["sr_filter_pos_beta"]      = config.sr_filter_pos_beta;
    j["sr_filter_rot_mincutoff"] = config.sr_filter_rot_mincutoff;
    j["sr_filter_rot_beta"]      = config.sr_filter_rot_beta;
    j["sr_angle_deadzone_deg"]   = config.sr_angle_deadzone_deg;
    j["sr_sens_yaw"]             = config.sr_sens_yaw;
    j["sr_sens_pitch"]           = config.sr_sens_pitch;
    j["sr_sens_roll"]            = config.sr_sens_roll;
    j["sr_max_yaw"]              = config.sr_max_yaw;
    j["sr_max_pitch"]            = config.sr_max_pitch;
    j["sr_max_roll"]             = config.sr_max_roll;
    j["sr_track_mode"]           = config.sr_track_mode;

    // Scripting
    j["launch_script"]   = config.launch_script;

    // Controller key bindings (string form is what the JSON stores)
    j["pose_reset_key"]  = config.pose_reset_str;
    j["ctrl_toggle_key"] = config.ctrl_toggle_str;
    j["ctrl_toggle_type"]= config.ctrl_type_str;
    j["pitch_radius"]    = config.pitch_radius;
    j["ctrl_deadzone"]   = config.ctrl_deadzone;
    j["ctrl_sensitivity"]= config.ctrl_sensitivity;

    // user_settings array — write whatever's live, fall back to defaults if
    // none configured. Scalar form for single-preset rows, array for cycles.
    auto writePreset = [](nlohmann::ordered_json& obj, const char* key,
                          const std::vector<float>& vals) {
        if (vals.size() == 1) obj[key] = vals[0];
        else                  obj[key] = vals;
    };
    if (config.num_user_settings > 0) {
        nlohmann::ordered_json arr = nlohmann::ordered_json::array();
        for (size_t i = 0; i < config.num_user_settings; ++i) {
            nlohmann::ordered_json u;
            u["user_load_key"]    = config.user_load_str[i];
            u["user_key_type"]    = config.user_type_str[i];
            writePreset(u, "user_depth",       config.user_depth[i]);
            writePreset(u, "user_convergence", config.user_convergence[i]);
            writePreset(u, "user_fov",         config.user_fov[i]);
            arr.push_back(u);
        }
        j["user_settings"] = arr;
    } else {
        j["user_settings"] = default_config_.at("user_settings");
    }

    // Re-key in canonical default_config_ order so the file stays diff-friendly
    // and any keys we forgot above get backfilled from the defaults.
    nlohmann::ordered_json merged = reorderFillJson(j);
    writeJsonToFile(filename, merged);
}


