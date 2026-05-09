#pragma once

#ifndef LYRA_LYRA_RENDER_FRAME_GRAPH_TRAITS_H
#define LYRA_LYRA_RENDER_FRAME_GRAPH_TRAITS_H

#include <type_traits>

namespace lyra
{
    template <typename T, typename = void>
    struct has_pre_read : std::false_type
    {
    };

    template <typename T>
    struct has_pre_read<T, typename std::enable_if<std::is_member_function_pointer<decltype(&T::pre_read)>::value>::type> : std::true_type
    {
    };

    template <typename T, typename = void>
    struct has_pre_write : std::false_type
    {
    };

    template <typename T>
    struct has_pre_write<T, typename std::enable_if<std::is_member_function_pointer<decltype(&T::pre_write)>::value>::type> : std::true_type
    {
    };
} // namespace lyra

#endif // LYRA_LYRA_RENDER_FRAME_GRAPH_TRAITS_H
