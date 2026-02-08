#pragma once

#ifndef LYRA_LIBRARY_COMMON_UUID_H
#define LYRA_LIBRARY_COMMON_UUID_H

#include <random>
#include <cinttypes>

#include <Lyra/Common/Hash.h>
#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>

namespace lyra
{
    // UUID is 128 bit integer
    static constexpr uint UUID_BYTES        = 16;
    static constexpr uint UUID_STRING_BYTES = 36;
    static constexpr uint UUID_STRING_COUNT = UUID_STRING_BYTES + 1;

    struct UUID
    {
        ulong hi = 0;
        ulong lo = 0;

        bool valid() const { return hi != 0 || lo != 0; }

        friend bool operator==(const UUID& lhs, const UUID& rhs)
        {
            return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
        }

        friend bool operator!=(const UUID& lhs, const UUID& rhs)
        {
            return !(lhs == rhs);
        }
    };

    namespace detail
    {
        constexpr int hexdigit_to_value(char c)
        {
            // clang-format off
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            // clang-format on
            return -1;
        }

        constexpr uint64_t parse_hex_qword_impl(const char* str, int start, int end, int i, uint64_t value)
        {
            // clang-format off
            return (i >= end) ? value : (str[i] == '-')
                ? parse_hex_qword_impl(str, start, end, i + 1, value)
                : parse_hex_qword_impl(str, start, end, i + 1, (value << 4) | hexdigit_to_value(str[i]));
            // clang-format on
        }

        constexpr uint64_t parse_hex_qword(const char* str, int start, int end)
        {
            return parse_hex_qword_impl(str, start, end, start, 0);
        }

        template <std::size_t... I>
        constexpr UUID make_uuid_impl(const char (&str)[UUID_STRING_COUNT], std::index_sequence<I...>)
        {
            return UUID{parse_hex_qword(str, 0, 18), parse_hex_qword(str, 18, 36)};
        }

    } // namespace detail

    constexpr UUID make_uuid(const char (&str)[UUID_STRING_COUNT])
    {
        return detail::make_uuid_impl(str, std::make_index_sequence<0>{});
    }

    FORCE_INLINE String to_string(const UUID& uuid)
    {
        String result(UUID_STRING_BYTES, ' ');
        snprintf(result.data(),
            UUID_STRING_COUNT,
            "%08x-%04x-%04x-%04x-%012" PRIx64,
            (uint32_t)((uuid.hi >> 32) & 0xFFFFFFFF),
            (uint16_t)((uuid.hi >> 16) & 0xFFFF),
            (uint16_t)(uuid.hi & 0xFFFF),
            (uint16_t)((uuid.lo >> 48) & 0xFFFF),
            (uint64_t)(uuid.lo & 0xFFFFFFFFFFFFull));
        return result;
    }

    FORCE_INLINE UUID random_uuid()
    {
        static std::random_device                      rd;
        static std::mt19937_64                         gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        return {dis(gen), dis(gen)};
    }

} // namespace lyra

namespace std
{
    template <>
    struct hash<lyra::UUID>
    {
        std::size_t operator()(const lyra::UUID& uuid) const noexcept
        {
            std::size_t hash = 1469598103934665603ull;
            lyra::hash_combine(hash, uuid.hi);
            lyra::hash_combine(hash, uuid.lo);
            return hash;
        }
    };
} // namespace std

#endif // LYRA_LIBRARY_COMMON_UUID_H
