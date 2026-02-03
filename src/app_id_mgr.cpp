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
#include <windows.h>
#include <fstream>
#include <regex>

#include "vrto3dlib/app_id_mgr.h"
#include "vrto3dlib/debug_log.hpp"
#include "vrto3dlib/win32_helper.hpp"

AppIdMgr::AppIdMgr() {
    steam_path_ = GetSteamInstallPath();
}


//-----------------------------------------------------------------------------
// Purpose: Parse Game's App ID from VRServer Log
//-----------------------------------------------------------------------------
std::vector<std::string> AppIdMgr::GetSteamAppIDs() {
    if (steam_path_.empty()) {
        LOG() << "Steam install path is empty. Cannot read logs.";
        return {};
    }

    std::string logFilePath = steam_path_ + "\\logs\\vrserver.txt";
    std::ifstream logFile(logFilePath);
    if (!logFile) {
        LOG() << "Failed to open log file: " << logFilePath;
        return {};
    }

    std::regex appkeyRegex(R"(SetApplicationPid.*appkey=(.*?)\s+pid=)");
    std::smatch match;
    std::string line;
    std::vector<std::string> appKeys;

    while (std::getline(logFile, line)) {
        if (std::regex_search(line, match, appkeyRegex) && match.size() > 1) {
            std::string key = match[1].str();
            if (excluded_app_keys_.find(key) == excluded_app_keys_.end()) {
                appKeys.push_back(key);
            }
        }
    }

    logFile.close();
    return appKeys;
}
