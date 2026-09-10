/** Versioned, allocation-free protocol shared by RetroShell and native
 * emulator adapters. This header deliberately contains no PSP APIs so its
 * path and receipt rules can be exercised by host tests. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace rs::nativeemu::protocol {

constexpr uint32_t VERSION = 1;
constexpr std::string_view SESSION_PREFIX = "ms0:/RETROSHELL/session/";
constexpr std::string_view SESSION_PATH =
    "ms0:/RETROSHELL/session/native-session.ini";

inline bool safeField(std::string_view value, size_t maxLength) {
    if (value.empty() || value.size() > maxLength) return false;
    for (unsigned char c : value)
        if (c < 0x20 || c == 0x7f || c == '=' || c == '\\') return false;
    return true;
}

inline bool safeSessionPath(std::string_view path) {
    return path == SESSION_PATH;
}

inline bool safeState(std::string_view state) {
    return state == "launched" || state == "paused" ||
           state == "resumed" || state == "returned" ||
           state == "reset" || state == "failed";
}

inline bool formatReceipt(char* out, size_t capacity,
                          std::string_view adapter, uint32_t romHash,
                          std::string_view state) {
    if (!out || capacity == 0 || !safeField(adapter, 48) ||
        !safeState(state))
        return false;
    const int chars = std::snprintf(
        out, capacity,
        "RETROSHELL_ADAPTER_SESSION=%u\nadapter=%.*s\nromHash=%08x\n"
        "state=%.*s\npauseHotkey=L+R+SELECT\n",
        unsigned(VERSION), int(adapter.size()), adapter.data(),
        unsigned(romHash), int(state.size()), state.data());
    return chars > 0 && size_t(chars) < capacity;
}

inline bool parseReceipt(std::string_view text, char* adapter,
                         size_t adapterCapacity, char* state,
                         size_t stateCapacity) {
    if (!adapter || !state || adapterCapacity < 2 || stateCapacity < 2 ||
        text.size() > 1024 ||
        text.find("RETROSHELL_ADAPTER_SESSION=1\n") != 0)
        return false;

    const auto read = [text](std::string_view key, char* out,
                             size_t capacity) {
        const size_t pos = text.find(key);
        if (pos == std::string_view::npos ||
            (pos != 0 && text[pos - 1] != '\n'))
            return false;
        const size_t start = pos + key.size();
        const size_t end = text.find('\n', start);
        if (end == std::string_view::npos || end == start ||
            end - start >= capacity)
            return false;
        const auto value = text.substr(start, end - start);
        std::memcpy(out, value.data(), value.size());
        out[value.size()] = 0;
        return true;
    };

    if (!read("adapter=", adapter, adapterCapacity) ||
        !read("state=", state, stateCapacity))
        return false;
    return safeField(adapter, 48) && safeState(state);
}

}  // namespace rs::nativeemu::protocol
