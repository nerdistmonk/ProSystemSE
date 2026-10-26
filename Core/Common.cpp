// ----------------------------------------------------------------------------
//   ___  ___  ___  ___       ___  ____  ___  _  _
//  /__/ /__/ /  / /__  /__/ /__    /   /_   / |/ /
// /    / \  /__/ ___/ ___/ ___/   /   /__  /    /  emulator
//
// ----------------------------------------------------------------------------
// Copyright 2005 Greg Stanton
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// ----------------------------------------------------------------------------
// Common.cpp
// ----------------------------------------------------------------------------
#include "Common.h"
#include <cstdio>
#include <algorithm>

std::string common_defaultPath = "";

std::string common_Format(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%f", value);
    return std::string(buffer);
}

std::string common_Format(uint value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%u", value);
    return std::string(buffer);
}

std::string common_Format(word value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<uint>(value));
    return std::string(buffer);
}

std::string common_Format(byte value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<uint>(value));
    return std::string(buffer);
}

std::string common_Format(bool value) {
    return value ? "true" : "false";
}

std::string common_Trim(std::string target) {
    size_t first = target.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = target.find_last_not_of(" \t\n\r");
    return target.substr(first, (last - first + 1));
}

std::string common_Remove(std::string target, char value) {
    target.erase(std::remove(target.begin(), target.end(), value), target.end());
    return target;
}

std::string common_Replace(std::string target, char value1, char value2) {
    std::replace(target.begin(), target.end(), value1, value2);
    return target;
}

std::string common_GetExtension(std::string filename) {
    size_t idx = filename.rfind('.');
    if (idx != std::string::npos) {
        return filename.substr(idx);
    }
    return "";
}

uint common_ParseUint(std::string text) {
    try {
        return static_cast<uint>(std::stoul(text, nullptr, 0));
    } catch (...) {
        return 0;
    }
}

word common_ParseWord(std::string text) {
    try {
        return static_cast<word>(std::stoul(text, nullptr, 0));
    } catch (...) {
        return 0;
    }
}

byte common_ParseByte(std::string text) {
    try {
        return static_cast<byte>(std::stoul(text, nullptr, 0));
    } catch (...) {
        return 0;
    }
}

bool common_ParseBool(std::string text) {
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower == "true" || lower == "1" || lower == "yes");
}
