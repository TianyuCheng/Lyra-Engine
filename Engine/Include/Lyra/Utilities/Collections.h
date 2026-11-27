#pragma once

#ifndef LYRA_ENGINE_UTILITIES_COLLECTIONS_H
#define LYRA_ENGINE_UTILITIES_COLLECTIONS_H

#include <list>
#include <array>
#include <deque>
#include <queue>
#include <span>
#include <stack>
#include <vector>
#include <variant>
#include <optional>
#include <forward_list>
#include <initializer_list>
#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <apus/slot_map.hpp>
#include <apus/ring_buffer.hpp>
#include <apus/small_vector.hpp>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Result.h>
#include <Lyra/Utilities/Detail/Blackboard.h>
#include <Lyra/Utilities/Detail/Services.h>

namespace lyra
{
    template <typename... T>
    using List = std::list<T...>;

    template <typename... T>
    using FList = std::forward_list<T...>;

    template <typename... T>
    using Deque = std::deque<T...>;

    template <typename... T>
    using Queue = std::queue<T...>;

    template <typename... T>
    using PriorityQueue = std::priority_queue<T...>;

    template <typename... T>
    using Stack = std::stack<T...>;

    template <typename T, int N>
    using Array = std::array<T, N>;

    template <typename... T>
    using Vector = std::vector<T...>;

    template <typename... T>
    using HashSet = absl::flat_hash_set<T...>;

    template <typename... T>
    using HashMap = absl::flat_hash_map<T...>;

    template <typename... T>
    using TreeMap = absl::btree_map<T...>;

    template <typename... T>
    using MultiMap = absl::btree_multimap<T...>;

    template <typename... T>
    using Variant = std::variant<T...>;

    template <typename... T>
    using Optional = std::optional<T...>;

    template <typename... T>
    using InitList = std::initializer_list<T...>;

    template <typename... T>
    using RingBuffer = apus::ring_buffer<T...>;

    template <typename... T>
    using Slotmap = apus::slot_map<T...>;

    template <typename T, usize N>
    using SmallVector = apus::small_vector<T, N>;

    template <typename T, usize Extent = std::dynamic_extent>
    struct Span : public std::span<T, Extent>
    {
        using Base = std::span<T, Extent>;
        using Base::Base;

        constexpr Span() noexcept = default;
        constexpr Span(const Base& other) noexcept : Base(other) {}
        constexpr Span(const T* ptr, usize count) noexcept : Base(const_cast<T*>(ptr), count) {}

        // allow construction from containers (vector, array, etc.)
        template <typename Container, typename = std::enable_if_t<
                                          !std::is_same_v<std::decay_t<Container>, Span> &&
                                          !std::is_same_v<std::decay_t<Container>, Base> &&
                                          requires(Container& c) { std::data(c); std::size(c); }>>
        constexpr Span(Container&& cont) noexcept
            : Base(const_cast<T*>(std::data(cont)), static_cast<usize>(std::size(cont))) {}

        // allow construction from a single element
        template <typename U, typename = std::enable_if_t<
                                  !std::is_array_v<U> &&
                                  !requires(U& u) { std::data(u); } &&
                                  std::is_convertible_v<U*, const T*>>>
        constexpr Span(U& single) noexcept : Base(const_cast<T*>(std::addressof(single)), 1) {}

        constexpr auto at(usize i) const -> typename Base::reference
        {
            if (i >= this->size()) {
                throw std::out_of_range("Span::at: index out of range");
            }
            return (*this)[i];
        }
    };

    using Blackboard = lyra::detail::Blackboard;
    using Services   = lyra::detail::Services;

} // end of namespace lyra

#endif // LYRA_ENGINE_UTILITIES_COLLECTIONS_H
