#pragma once

#ifndef LYRA_LIBRARY_COMMON_CONTAINER_H
#define LYRA_LIBRARY_COMMON_CONTAINER_H

#include <map>
#include <list>
#include <array>
#include <deque>
#include <stack>
#include <vector>
#include <optional>
#include <forward_list>
#include <unordered_set>
#include <unordered_map>
#include <initializer_list>
#include <boost/circular_buffer.hpp>
#include <Lyra/Common/Slotmap.h>

namespace lyra
{
    template <typename... T>
    using List = std::list<T...>;

    template <typename... T>
    using FList = std::forward_list<T...>;

    template <typename... T>
    using Deque = std::deque<T...>;

    template <typename... T>
    using Stack = std::stack<T...>;

    template <typename T, int N>
    using Array = std::array<T, N>;

    template <typename... T>
    using Vector = std::vector<T...>;

    template <typename... T>
    using HashSet = std::unordered_set<T...>;

    template <typename... T>
    using HashMap = std::unordered_map<T...>;

    template <typename... T>
    using TreeMap = std::map<T...>;

    template <typename... T>
    using Optional = std::optional<T...>;

    template <typename... T>
    using InitList = std::initializer_list<T...>;

    template <typename Elem>
    using RingBuffer = boost::circular_buffer<Elem>;

} // end of namespace lyra

#endif // LYRA_LIBRARY_COMMON_CONTAINER_H
