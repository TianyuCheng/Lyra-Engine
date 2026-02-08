#pragma once

#ifndef LYRA_LIBRARY_COMMON_ASSERT_H
#define LYRA_LIBRARY_COMMON_ASSERT_H

#include <libassert/assert.hpp>

#ifndef assert
#define assert(...) DEBUG_ASSERT(__VA_ARGS__)
#endif

#endif // LYRA_LIBRARY_COMMON_ASSERT_H
