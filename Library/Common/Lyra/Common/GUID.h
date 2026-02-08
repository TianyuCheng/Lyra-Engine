#pragma once

#ifndef LYRA_LIBRARY_COMMON_GUID_H
#define LYRA_LIBRARY_COMMON_GUID_H

#include <random>

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
} // namespace lyra

#endif // LYRA_LIBRARY_COMMON_GUID_H
