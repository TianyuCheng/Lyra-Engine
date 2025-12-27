#pragma once

#ifndef LYRA_LIBRARY_COMMON_COMPATIBILITY_H
#define LYRA_LIBRARY_COMMON_COMPATIBILITY_H

// NOTE: This header is going to be used by other projects,
// Use something universal (instead of Lyra's internal macro for OS check).
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min          // conflicts with std::min
#undef max          // conflicts with std:max
#undef near         // commonly used words
#undef far          // commonly used words
#undef GENERIC_READ // conflicts with RHI enum
#undef OPAUE
#undef DEBUG
#else
#endif

#ifdef __APPLE__
#undef DEBUG
#endif

#include <optional>    // for std::optional
#include <string_view> // for std::string_view
#include <string>      // for std::string
#include <cstdlib>     // IWYU pragma: keep

namespace lyra
{
    // Helper function to get environment variables safely and portably
    inline std::optional<std::string> get_environment_variable(std::string_view key)
    {
#if defined(_WIN32)
        // Max size for environment variable value is 32767 characters
        char  buffer[32768];
        DWORD len = GetEnvironmentVariableA(key.data(), buffer, sizeof(buffer));
        if (len == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return std::nullopt;
        if (len == 0 || len >= sizeof(buffer)) return std::nullopt;
        return std::string(buffer, len);
#else
        const char* v = std::getenv(key.data());
        if (!v) return std::nullopt;
        return std::string(v);
#endif
    }
} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_COMPATIBILITY_H
