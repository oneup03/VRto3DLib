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

#include <fstream>
#include <sstream>
#include <string>
#include <windows.h>

std::string GetSteamInstallPath();

class DebugLog {
public:
    DebugLog() = default;

    ~DebugLog() {
        flush();
    }

    // Generic stream operator
    template<typename T>
    DebugLog& operator<<(const T& v) {
        stream_ << v;
        return *this;
    }

    // Narrow string overload (auto widen)
    DebugLog& operator<<(const char* s) {
        widen(s);
        return *this;
    }

    DebugLog& operator<<(const std::string& s) {
        widen(s.c_str());
        return *this;
    }

private:
    struct FileState {
        bool enabled = false;
        std::string current_log_path;
    };

    struct FileInitScope {
        FileInitScope() {
            InProgress() = true;
        }

        ~FileInitScope() {
            InProgress() = false;
        }

        static bool& InProgress() {
            static thread_local bool in_progress = false;
            return in_progress;
        }
    };

    static std::string ToUtf8(const std::wstring& message) {
        if (message.empty()) {
            return {};
        }

        const int required_size = WideCharToMultiByte(
            CP_UTF8,
            0,
            message.c_str(),
            static_cast<int>(message.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

        if (required_size <= 0) {
            return {};
        }

        std::string utf8(static_cast<size_t>(required_size), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            message.c_str(),
            static_cast<int>(message.size()),
            utf8.data(),
            required_size,
            nullptr,
            nullptr);

        return utf8;
    }

    static FileState InitializeFileState() {
        FileInitScope init_scope;
        FileState state;

        const std::string steam_path = GetSteamInstallPath();
        if (steam_path.empty()) {
            return state;
        }

        const std::string logs_dir = steam_path + "\\logs";
        const DWORD attrs = GetFileAttributesA(logs_dir.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            return state;
        }

        const std::string current_log_path = logs_dir + "\\vrto3d.txt";
        const std::string previous_log_path = logs_dir + "\\vrto3d_previous.txt";

        DeleteFileA(previous_log_path.c_str());
        MoveFileExA(current_log_path.c_str(), previous_log_path.c_str(), MOVEFILE_REPLACE_EXISTING);

        std::ofstream new_log(current_log_path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!new_log.is_open()) {
            return state;
        }

        state.enabled = true;
        state.current_log_path = current_log_path;
        return state;
    }

    static const FileState& GetFileState() {
        static const FileState state = InitializeFileState();
        return state;
    }

    static void AppendToFile(const std::wstring& message) {
        if (FileInitScope::InProgress()) {
            return;
        }

        const FileState& state = GetFileState();
        if (!state.enabled || state.current_log_path.empty()) {
            return;
        }

        std::ofstream log_file(state.current_log_path, std::ios::out | std::ios::app | std::ios::binary);
        if (!log_file.is_open()) {
            return;
        }

        const std::string utf8 = ToUtf8(message);
        if (!utf8.empty()) {
            log_file << utf8;
        }
    }

    void flush() {
        if (stream_.tellp() == 0)
            return;

        stream_ << L'\n';
        const std::wstring message = stream_.str();
        OutputDebugStringW(message.c_str());
        AppendToFile(message);
    }

    void widen(const char* s) {
        if (!s) return;
        while (*s)
            stream_ << static_cast<wchar_t>(*s++);
    }

    std::wstringstream stream_;
};

#define LOG() DebugLog()
