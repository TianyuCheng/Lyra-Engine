#pragma once

#ifndef LYRA_LIBRARY_COMMON_DETAIL_SLOTMAP_H
#define LYRA_LIBRARY_COMMON_DETAIL_SLOTMAP_H

#include <vector>
#include <cstdint>

namespace lyra::detail
{
    // primary template - defaults to false
    template <typename T, typename = void>
    struct has_valid : std::false_type
    {
    };

    // specialization that checks for valid() -> bool
    template <typename T>
    struct has_valid<T, std::void_t<decltype(std::declval<T>().valid())>> : std::bool_constant<std::is_same_v<decltype(std::declval<T>().valid()), bool>>
    {
    };

    template <typename T, typename deleter_type, typename index_type = std::uint32_t>
    struct slotmap;

    template <typename T, bool IsConst>
    struct slotmap_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = std::conditional_t<IsConst, const T*, T*>;
        using reference         = std::conditional_t<IsConst, const T&, T&>;

        slotmap_iterator(pointer ptr, size_t index, size_t size) : m_ptr(ptr), m_index(index), m_size(size)
        {
            while (m_index < m_size && !m_ptr[m_index].valid())
                m_index++;
        }

        reference operator*() const { return m_ptr[m_index]; }
        pointer   operator->() { return &m_ptr[m_index]; }

        slotmap_iterator& operator++()
        {
            m_index++;
            while (m_index < m_size && !m_ptr[m_index].valid())
                m_index++;
            return *this;
        }

        slotmap_iterator operator++(int)
        {
            slotmap_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const slotmap_iterator& a, const slotmap_iterator& b) { return a.m_index == b.m_index; };
        friend bool operator!=(const slotmap_iterator& a, const slotmap_iterator& b) { return a.m_index != b.m_index; };

    private:
        pointer m_ptr;
        size_t  m_index;
        size_t  m_size;
    };

    template <typename T, typename deleter_type, typename index_type>
    struct slotmap
    {
    private:
        std::vector<T>          data = {};
        std::vector<index_type> free = {};
        index_type              rear = 0;

    public:
        using iterator       = slotmap_iterator<T, false>;
        using const_iterator = slotmap_iterator<T, true>;

        virtual ~slotmap() { clear(); }

        void clear()
        {
            for (auto& item : data)
                if (item.valid())
                    deleter_type()(item);
        }

        auto add(const T& value) -> index_type
        {
            index_type index = get_next_index();

            data.at(index) = value;
            return index;
        }

        void remove(index_type index)
        {
            assert(index < data.size());
            deleter_type()(data.at(index));
            free.push_back(index);
        }

        auto at(index_type index) -> T& { return data.at(index); }
        auto at(index_type index) const -> const T& { return data.at(index); }

        auto begin() -> iterator { return iterator(data.data(), 0, data.size()); }
        auto end() -> iterator { return iterator(data.data(), data.size(), data.size()); }

        auto cbegin() -> const_iterator const { return const_iterator(data.data(), 0, data.size()); }
        auto cend() -> const_iterator const { return const_iterator(data.data(), data.size(), data.size()); }

        bool range_check(index_type index) const { return index < data.size(); }

    private:
        auto get_next_index() -> index_type
        {
            // fallback: increase the data array
            if (free.empty()) {
                if (rear < data.size())
                    return rear++;

                resize_data();
                return rear++;
            }

            // find from free list
            index_type index = free.back();
            free.pop_back();
            return index;
        }

        void resize_data()
        {
            size_t old_size = data.size();
            size_t tgt_size = old_size * 2 + 1;
            size_t new_size = tgt_size > 2048 ? 2048 : tgt_size;
            data.resize(new_size);
            rear = static_cast<index_type>(old_size);
        }
    };

} // namespace lyra::detail

#endif // LYRA_LIBRARY_COMMON_DETAIL_SLOTMAP_H
