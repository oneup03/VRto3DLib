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

#include <sstream>
#include <string>
#include <windows.h>

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
    void flush() {
        if (stream_.tellp() == 0)
            return;

        stream_ << L'\n';
        OutputDebugStringW(stream_.str().c_str());
    }

    void widen(const char* s) {
        if (!s) return;
        while (*s)
            stream_ << static_cast<wchar_t>(*s++);
    }

    std::wstringstream stream_;
};

#define LOG() DebugLog()
