#ifndef LYRA_TESTLIB_HELPER_COMMON_H
#define LYRA_TESTLIB_HELPER_COMMON_H

#include <Lyra/Lyra.hpp>

using namespace lyra;

template <typename T>
T align(T value, T alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

#endif // LYRA_TESTLIB_HELPER_COMMON_H
