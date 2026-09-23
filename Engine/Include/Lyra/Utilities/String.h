#pragma once

#ifndef LYRA_ENGINE_UTILITIES_STRING_H
#define LYRA_ENGINE_UTILITIES_STRING_H

#include <cstdio>
#include <cstring>
#include <vector>
#include <locale>
#include <string>
#include <string_view>

#if !defined(_WIN32)
// to allow compilation on non-Windows platforms (e.g. MacOS, Linux)
inline size_t strnlen_s(const char* s, size_t maxlen)
{
    if (!s) return 0;
    return strnlen(s, maxlen);
}

inline int strncpy_s(char* dest, size_t destsz, const char* src, size_t count)
{
    if (!dest || destsz == 0) return 22; // EINVAL
    if (!src) {
        dest[0] = '\0';
        return 22; // EINVAL
    }
    size_t n = count < (destsz - 1) ? count : (destsz - 1);
    size_t i = 0;
    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
    return 0;
}

#ifndef sprintf_s
#define sprintf_s(buf, size, ...) snprintf(buf, size, __VA_ARGS__)
#endif
#endif

namespace lyra
{
    using String  = std::string;
    using WString = std::wstring;
    using CString = const char*;

    using StringView = std::string_view;

    // for windows compatibility
    using LPCWSTR = const wchar_t*;

    // conversion from const char* to wstring
    inline std::wstring to_wstring(const char* str)
    {
        size_t size = strnlen_s(str, 32);

        std::vector<wchar_t> buf(size);
        std::use_facet<std::ctype<wchar_t>>(std::locale()).widen(str, str + size, buf.data());
        return std::wstring(buf.data(), buf.size());
    }

    // conversion from string to wstring
    inline std::wstring to_wstring(const std::string& str)
    {
        std::vector<wchar_t> buf(str.size());
        std::use_facet<std::ctype<wchar_t>>(std::locale()).widen(str.data(), str.data() + str.size(), buf.data());
        return std::wstring(buf.data(), buf.size());
    }

    // identitity conversion: string to string
    inline std::string to_string(const std::string& str)
    {
        return str;
    }

    // convert wstring to string
    inline std::string to_string(const std::wstring& str, const std::locale& loc = std::locale{})
    {
        std::vector<char> buf(str.size());
        std::use_facet<std::ctype<wchar_t>>(loc).narrow(str.data(), str.data() + str.size(), '?', buf.data());
        return std::string(buf.data(), buf.size());
    }

    /**
     * @brief Fixed-capacity, null-terminated inline string buffer with strict C-ABI layout.
     *
     * Standard-layout, exactly N bytes in size (binary-compatible with struct { char data[N]; } in C),
     * avoiding any dynamic allocations or pointer indirection across C ABI boundaries.
     */
    template <size_t N>
    struct FixedString
    {
        static_assert(N > 0, "FixedString capacity must be greater than zero");

        char data[N] = {};

        constexpr FixedString() = default;

        FixedString(const char* str)
        {
            if (str) {
                strncpy_s(data, N, str, N - 1);
            }
        }

        FixedString(StringView str)
        {
            size_t len = (str.size() < N - 1) ? str.size() : (N - 1);
            memcpy(data, str.data(), len);
            data[len] = '\0';
        }

        FixedString& operator=(const char* str)
        {
            if (str) {
                strncpy_s(data, N, str, N - 1);
            } else {
                data[0] = '\0';
            }
            return *this;
        }

        FixedString& operator=(StringView str)
        {
            size_t len = (str.size() < N - 1) ? str.size() : (N - 1);
            memcpy(data, str.data(), len);
            data[len] = '\0';
            return *this;
        }

        constexpr const char* c_str() const noexcept { return data; }
        constexpr const char* data_ptr() const noexcept { return data; }
        constexpr char* data_ptr() noexcept { return data; }
        constexpr operator const char*() const noexcept { return data; }
        operator StringView() const noexcept { return StringView(data); }

        constexpr size_t capacity() const noexcept { return N; }
        size_t size() const noexcept { return strnlen_s(data, N); }
        constexpr bool empty() const noexcept { return data[0] == '\0'; }

        bool operator==(const char* other) const noexcept { return other && strcmp(data, other) == 0; }
        bool operator==(StringView other) const noexcept { return StringView(data) == other; }
        bool operator==(const FixedString& other) const noexcept { return strcmp(data, other.data) == 0; }
    };

    static_assert(std::is_standard_layout_v<FixedString<64>>);
    static_assert(sizeof(FixedString<64>) == 64);

    using FixedString64  = FixedString<64>;
    using FixedString128 = FixedString<128>;
    using FixedString256 = FixedString<256>;
} // namespace lyra

#endif // LYRA_ENGINE_UTILITIES_STRING_H
