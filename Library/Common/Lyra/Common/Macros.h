#pragma once

#ifndef LYRA_LIBRARY_COMMON_MACROS_H
#define LYRA_LIBRARY_COMMON_MACROS_H

// force inline
#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif

// debug tools (this is copied from ImGui)
// This will call DEBUG_BREAK() which you may redefine yourself. See https://github.com/scottt/debugbreak for more reference.
#ifndef DEBUG_BREAK
#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
#define DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#define DEBUG_BREAK() __asm__ volatile("int3;nop")
#elif defined(__GNUC__) && defined(__thumb__)
#define DEBUG_BREAK() __asm__ volatile(".inst 0xde01")
#elif defined(__GNUC__) && defined(__arm__) && !defined(__thumb__)
#define DEBUG_BREAK() __asm__ volatile(".inst 0xe7f001f0")
#else
#define DEBUG_BREAK() assert(0) // It is expected that you define DEBUG_BREAK() into something that will break nicely in a debugger!
#endif
#endif // #ifndef DEBUG_BREAK

#ifndef MAYBE_UNUSED
#define MAYBE_UNUSED(V) (void)(V)
#endif

#ifndef UNIMPLEMENTED
#define UNIMPLEMENTED(MESSAGE)                         \
    {                                                  \
        spdlog::error("NOT IMPLEMENTED: {}", MESSAGE); \
        DEBUG_BREAK();                                 \
    }
#endif

#endif // LYRA_LIBRARY_COMMON_MACROS_H
