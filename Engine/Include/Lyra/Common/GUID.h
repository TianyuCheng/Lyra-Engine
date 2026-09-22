#pragma once

#ifndef LYRA_LYRA_COMMON_GUID_H
#define LYRA_LYRA_COMMON_GUID_H

#include <random>
#include <string_view>

#include <Lyra/Common/Macros.h>
#include <Lyra/Common/Stdint.h>

namespace lyra
{
    using GUID = ulong;

    FORCE_INLINE GUID random_guid()
    {
        static std::random_device                      rd;
        static std::mt19937_64                         gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        return dis(gen);
    }

    /**
     * @brief Deterministically generates a stable 64-bit GUID
     *        derived from parent GUID and a local tag/name.
     */
    inline GUID deterministic_guid(GUID parent_guid, std::string_view name)
    {
        constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
        constexpr uint64_t FNV_PRIME        = 1099511628211ull;

        uint64_t hash = FNV_OFFSET_BASIS;

        auto hash_bytes = [&](const void* data, size_t len) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < len; ++i) {
                hash ^= bytes[i];
                hash *= FNV_PRIME;
            }
        };

        hash_bytes(&parent_guid, sizeof(parent_guid));
        hash_bytes(name.data(), name.size());

        // splitmix64 avalanche mixer
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ull;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebull;
        hash ^= hash >> 31;

        return (hash == 0) ? 1ull : hash;
    }

    /**
     * @brief Generates a deterministic probe GUID in case of a collision.
     */
    inline GUID probe_guid(GUID current_guid, uint32_t attempt)
    {
        constexpr uint64_t PROBE_SALT = 0x9e3779b97f4a7c15ull;

        uint64_t hash = current_guid ^ (PROBE_SALT * static_cast<uint64_t>(attempt + 1));
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ull;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebull;
        hash ^= hash >> 31;
        return (hash == 0) ? 1ull : hash;
    }
} // namespace lyra

#endif // LYRA_LYRA_COMMON_GUID_H
