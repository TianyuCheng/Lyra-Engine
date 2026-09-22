#pragma once

#ifndef LYRA_ENGINE_UTILITIES_ASSERT_H
#define LYRA_ENGINE_UTILITIES_ASSERT_H

#include <libassert/assert.hpp>

#ifndef assert
#define assert(...) DEBUG_ASSERT(__VA_ARGS__)
#endif

#endif // LYRA_ENGINE_UTILITIES_ASSERT_H
