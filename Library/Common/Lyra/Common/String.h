#pragma once

#ifndef LYRA_LIBRARY_COMMON_STRING_H
#define LYRA_LIBRARY_COMMON_STRING_H

#include <vector>
#include <locale>
#include <string>
#include <string_view>

#if __APPLE__
// to allow compilation on MacOS
size_t strnlen_s(const char* s, size_t maxlen);
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
} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_STRING_H
