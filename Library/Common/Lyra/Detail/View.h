#pragma once

#ifndef LYRA_LIBRARY_COMMON_DETAIL_VIEW_H
#define LYRA_LIBRARY_COMMON_DETAIL_VIEW_H

#include <vector>
#include <stdexcept>

namespace lyra::detail
{
    struct untyped_view
    {
        void*  data = nullptr;
        size_t size = 0;
    };

    template <typename T>
    struct typed_view
    {
        // Iterator type aliases
        using iterator               = T*;
        using const_iterator         = const T*;
        using reverse_iterator       = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        typed_view() : data_(nullptr), count(0ull) {}

        typed_view(T& data) : data_(const_cast<T*>(&data)), count(1) {}
        typed_view(const T&& data) = delete;
        typed_view(T&&)            = delete;

        typed_view(const std::vector<T>& data) : data_(const_cast<T*>(data.data())), count(data.size()) {}
        typed_view(const std::vector<T>&&) = delete;
        typed_view(std::vector<T>&&)       = delete;

        template <size_t N>
        typed_view(T (&data)[N]) : data_(data), count(N) {}

        template <size_t N>
        typed_view(const T (&data)[N]) : data_(const_cast<T*>(data)), count(N) {}

        template <size_t N>
        typed_view(const std::array<T, N>& data) : data_(const_cast<T*>(data.data())), count(N) {}

        template <size_t N>
        typed_view(std::array<T, N>&&) = delete;

        template <size_t N>
        typed_view(const std::array<T, N>&&) = delete;

        typed_view(std::initializer_list<T>) = delete;

        size_t size() const { return count; }

        // Forward iterator methods
        iterator begin() noexcept { return data_; }
        iterator end() noexcept { return data_ + count; }

        const_iterator begin() const noexcept { return data_; }
        const_iterator end() const noexcept { return data_ + count; }

        const_iterator cbegin() const noexcept { return data_; }
        const_iterator cend() const noexcept { return data_ + count; }

        // Reverse iterator methods
        reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
        reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

        // additional utility methods for completeness
        bool empty() const noexcept { return count == 0; }

        T*       data() { return data_; }
        const T* data() const { return data_; }

        T&       operator[](size_t index) noexcept { return data_[index]; }
        const T& operator[](size_t index) const noexcept { return data_[index]; }

        T& at(size_t index)
        {
            if (index >= count) {
                throw std::out_of_range("typed_view::at: index out of range");
            }
            return data_[index];
        }

        const T& at(size_t index) const
        {
            if (index >= count) {
                throw std::out_of_range("typed_view::at: index out of range");
            }
            return data_[index];
        }

        T&       front() noexcept { return data_[0]; }
        const T& front() const noexcept { return data_[0]; }

        T&       back() noexcept { return data_[count - 1]; }
        const T& back() const noexcept { return data_[count - 1]; }

    private:
        T*     data_ = nullptr;
        size_t count = 0;
    };

} // namespace lyra::detail

#endif // LYRA_LIBRARY_COMMON_DETAIL_VIEW_H
