#pragma once

#include <vector>
#include <memory_resource>

namespace lyra::detail
{

    /**
     * @brief A linear memory allocator that uses pages of memory.
     *
     * This allocator is designed for fast, sequential allocations. It allocates memory in pages and can grow by adding new pages as needed.
     * It is not thread-safe.
     *
     * @tparam T The type of object to allocate.
     */
    template <typename T>
    class MemoryArena
    {
    public:
        /**
         * @brief Constructs a MemoryArena.
         *
         * @param page_size The number of objects of type T to allocate per page.
         * @param upstream The upstream memory resource to use for allocations. Defaults to std::pmr::get_default_resource().
         */
        explicit MemoryArena(size_t page_size, std::pmr::memory_resource* upstream = std::pmr::get_default_resource());

        /**
         * @brief Destroys the MemoryArena, freeing all allocated memory.
         */
        ~MemoryArena();

        MemoryArena(const MemoryArena&)            = delete;
        MemoryArena& operator=(const MemoryArena&) = delete;
        MemoryArena(MemoryArena&&) noexcept;
        MemoryArena& operator=(MemoryArena&&) noexcept;

        /**
         * @brief Allocates memory for a single object of type T.
         *
         * This function returns a pointer to uninitialized memory.
         *
         * @return A pointer to the allocated memory.
         */
        T* allocate();

        /**
         * @brief Resets the allocator, reclaiming all allocated memory without deallocating the pages.
         *
         * After a reset, new allocations will start from the beginning of the first page.
         * This does not call destructors on any objects that may have been constructed in the allocated memory.
         */
        void reset();

        /**
         * @brief Frees all allocated memory, including all pages.
         *
         * This does not call destructors on any objects that may have been constructed in the allocated memory.
         */
        void destroy();

    private:
        struct Page
        {
            void*  memory;
            size_t capacity; // capacity in bytes
        };

        void new_page();

        std::pmr::memory_resource* _upstream;
        std::vector<Page>          _pages;
        size_t                     _page_size;
        size_t                     _current_page_index = 0;
        size_t                     _current_offset     = 0;
    };

    template <typename T>
    MemoryArena<T>::MemoryArena(size_t page_size, std::pmr::memory_resource* upstream)
        : _upstream(upstream), _page_size(page_size)
    {
        new_page();
    }

    template <typename T>
    MemoryArena<T>::~MemoryArena()
    {
        destroy();
    }

    template <typename T>
    MemoryArena<T>::MemoryArena(MemoryArena&& other) noexcept
        : _upstream(other._upstream),
          _pages(std::move(other._pages)),
          _page_size(other._page_size),
          _current_page_index(other._current_page_index),
          _current_offset(other._current_offset)
    {
        other._pages.clear();
        other._page_size          = 0;
        other._current_page_index = 0;
        other._current_offset     = 0;
    }

    template <typename T>
    MemoryArena<T>& MemoryArena<T>::operator=(MemoryArena&& other) noexcept
    {
        if (this != &other) {
            destroy();
            _upstream           = other._upstream;
            _pages              = std::move(other._pages);
            _page_size          = other._page_size;
            _current_page_index = other._current_page_index;
            _current_offset     = other._current_offset;

            other._pages.clear();
            other._page_size          = 0;
            other._current_page_index = 0;
            other._current_offset     = 0;
        }
        return *this;
    }

    template <typename T>
    T* MemoryArena<T>::allocate()
    {
        if (_current_offset + sizeof(T) > _pages[_current_page_index].capacity) {
            _current_page_index++;
            _current_offset = 0;
            if (_current_page_index >= _pages.size()) {
                new_page();
            }
        }

        char* ptr = static_cast<char*>(_pages[_current_page_index].memory) + _current_offset;
        _current_offset += sizeof(T);

        return reinterpret_cast<T*>(ptr);
    }

    template <typename T>
    void MemoryArena<T>::reset()
    {
        _current_page_index = 0;
        _current_offset     = 0;
    }

    template <typename T>
    void MemoryArena<T>::destroy()
    {
        for (const auto& page : _pages) {
            _upstream->deallocate(page.memory, page.capacity, alignof(T));
        }
        _pages.clear();
        _current_page_index = 0;
        _current_offset     = 0;
    }

    template <typename T>
    void MemoryArena<T>::new_page()
    {
        const size_t capacity = _page_size * sizeof(T);
        void*        memory   = _upstream->allocate(capacity, alignof(T));
        _pages.push_back({memory, capacity});
    }

} // namespace lyra::detail
