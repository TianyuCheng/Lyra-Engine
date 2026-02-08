#pragma once

#ifndef LYRA_LIBRARY_COMMON_DETAIL_ARENA_H
#define LYRA_LIBRARY_COMMON_DETAIL_ARENA_H

#include <vector>
#include <memory_resource>

namespace lyra::detail
{

    /**
     * @brief A linear memory allocator that uses pages of memory.
     *
     * This allocator is designed for fast, sequential allocations. It allocates memory in pages and can grow by adding new pages as needed.
     * It is not thread-safe.
     */
    class MemoryArena
    {
    public:
        /**
         * @brief Constructs a MemoryArena.
         *
         * @param page_size The size of each page in bytes.
         * @param upstream The upstream memory resource to use for allocations. Defaults to std::pmr::get_default_resource().
         */
        explicit MemoryArena(size_t page_size_bytes, std::pmr::memory_resource* upstream = std::pmr::get_default_resource());

        /**
         * @brief Destroys the MemoryArena, freeing all allocated memory.
         */
        ~MemoryArena();

        MemoryArena(const MemoryArena&)            = delete;
        MemoryArena& operator=(const MemoryArena&) = delete;
        MemoryArena(MemoryArena&&) noexcept;
        MemoryArena& operator=(MemoryArena&&) noexcept;

        /**
         * @brief Allocates a block of memory from the arena.
         *
         * @param size The size of the memory block in bytes.
         * @param alignment The required alignment of the memory block.
         * @return A pointer to the allocated memory, or nullptr if allocation fails.
         */
        void* allocate(size_t size, size_t alignment);

        /**
         * @brief Allocates and constructs an object of type T.
         */
        template <typename T, typename... Args>
        T* allocate(Args&&... args)
        {
            void* memory = allocate(sizeof(T), alignof(T));
            if (!memory) {
                return nullptr;
            }
            return new (memory) T(std::forward<Args>(args)...);
        }

        /**
         * @brief Resets the allocator, reclaiming all allocated memory without deallocating the pages.
         */
        void reset();

        /**
         * @brief Frees all allocated memory, including all pages.
         */
        void destroy();

    private:
        struct Page
        {
            void*  memory;
            size_t capacity; // capacity in bytes
        };

        void new_page(size_t required_size);

        std::pmr::memory_resource* _upstream;
        std::vector<Page>          _pages;
        size_t                     _page_size_bytes;
        size_t                     _current_page_index = 0;
        size_t                     _current_offset     = 0;
    };

    inline MemoryArena::MemoryArena(size_t page_size_bytes, std::pmr::memory_resource* upstream)
        : _upstream(upstream), _page_size_bytes(page_size_bytes)
    {
        new_page(_page_size_bytes);
    }

    inline MemoryArena::~MemoryArena()
    {
        destroy();
    }

    inline MemoryArena::MemoryArena(MemoryArena&& other) noexcept
        : _upstream(other._upstream),
          _pages(std::move(other._pages)),
          _page_size_bytes(other._page_size_bytes),
          _current_page_index(other._current_page_index),
          _current_offset(other._current_offset)
    {
        other._pages.clear();
        other._page_size_bytes    = 0;
        other._current_page_index = 0;
        other._current_offset     = 0;
    }

    inline MemoryArena& MemoryArena::operator=(MemoryArena&& other) noexcept
    {
        if (this != &other) {
            destroy();
            _upstream           = other._upstream;
            _pages              = std::move(other._pages);
            _page_size_bytes    = other._page_size_bytes;
            _current_page_index = other._current_page_index;
            _current_offset     = other._current_offset;

            other._pages.clear();
            other._page_size_bytes    = 0;
            other._current_page_index = 0;
            other._current_offset     = 0;
        }
        return *this;
    }

    inline void* MemoryArena::allocate(size_t size, size_t alignment)
    {
        if (_pages.empty()) {
            new_page(size > _page_size_bytes ? size : _page_size_bytes);
        }

        uintptr_t current_ptr    = reinterpret_cast<uintptr_t>(_pages[_current_page_index].memory) + _current_offset;
        uintptr_t aligned_ptr    = (current_ptr + (alignment - 1)) & ~(alignment - 1);
        size_t    aligned_offset = aligned_ptr - reinterpret_cast<uintptr_t>(_pages[_current_page_index].memory);

        if (aligned_offset + size > _pages[_current_page_index].capacity) {
            _current_page_index++;
            if (_current_page_index >= _pages.size()) {
                new_page(size > _page_size_bytes ? size : _page_size_bytes);
            }
            _current_offset = 0;
            aligned_offset  = 0;
        }

        void* ptr       = static_cast<char*>(_pages[_current_page_index].memory) + aligned_offset;
        _current_offset = aligned_offset + size;

        return ptr;
    }

    inline void MemoryArena::reset()
    {
        _current_page_index = 0;
        _current_offset     = 0;
    }

    inline void MemoryArena::destroy()
    {
        for (const auto& page : _pages) {
            // This is problematic. We need to know the alignment used for allocation.
            // PMR requires the same alignment for deallocation. Let's assume a max alignment.
            constexpr size_t max_alignment = 16;
            _upstream->deallocate(page.memory, page.capacity, max_alignment);
        }
        _pages.clear();
        _current_page_index = 0;
        _current_offset     = 0;
    }

    inline void MemoryArena::new_page(size_t required_size)
    {
        constexpr size_t max_alignment = 16;
        size_t           capacity      = required_size > _page_size_bytes ? required_size : _page_size_bytes;
        void*            memory        = _upstream->allocate(capacity, max_alignment);
        if (memory) {
            _pages.push_back({memory, capacity});
        }
    }

} // namespace lyra::detail

#endif // LYRA_LIBRARY_COMMON_DETAIL_ARENA_H
