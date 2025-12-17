#ifndef LYRA_PLUGIN_D3D12_BLOCK_ALLOCATOR_H
#define LYRA_PLUGIN_D3D12_BLOCK_ALLOCATOR_H

#include <map>
#include <limits>
#include <cstddef>

// forward declaration for friend access
template <typename T>
class BlockAllocator;

/**
 * a typesafe handle representing a memory allocation.
 *
 * its constructor is private to prevent creation with arbitrary offsets.
 * only a blockallocator can create valid handles.
 */
class AllocationHandle
{
public:
    static constexpr size_t invalid_offset = std::numeric_limits<size_t>::max();

    // constructs an invalid handle.
    AllocationHandle() : offset(invalid_offset) {}

    // checks if the handle is valid.
    bool valid() const { return offset != invalid_offset; }

    bool operator==(const AllocationHandle& other) const { return offset == other.offset; }
    bool operator!=(const AllocationHandle& other) const { return offset != other.offset; }

    // retrieve the offset data
    auto get_offset() const { return offset; }

private:
    explicit AllocationHandle(size_t o) : offset(o) {}
    size_t offset;

    template <typename T>
    friend class BlockAllocator;
};

/**
 * a general-purpose block allocator that manages a virtual memory space.
 *
 * this allocator sub-allocates regions from a virtual space of a given capacity.
 * it uses a map to keep track of available memory segments and supports allocation
 * of variable-sized blocks. memory is automatically "recycled" on deallocation.
 * this allocator does not manage the actual memory buffer, only the layout.
 *
 * the template parameter `t` is used to ensure proper memory alignment for the allocated blocks.
 *
 * @tparam t the type of object that will be stored. this is used for alignment purposes.
 */
template <typename T = std::byte>
class BlockAllocator
{
private:
    size_t                   m_capacity;
    std::map<size_t, size_t> m_free_blocks;      // offset -> size
    std::map<size_t, size_t> m_allocated_blocks; // offset -> size

public:
    /**
     * constructs a blockallocator with a given capacity.
     * @param capacity the total number of bytes in the virtual memory pool.
     */
    explicit BlockAllocator() { init(0); }
    explicit BlockAllocator(size_t capacity)
    {
        init(capacity);
    }

    ~BlockAllocator() = default;

    BlockAllocator(const BlockAllocator&)            = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;
    BlockAllocator(BlockAllocator&&)                 = delete;
    BlockAllocator& operator=(BlockAllocator&&)      = delete;

    void init(size_t capacity)
    {
        m_capacity = capacity;
        if (capacity > 0) {
            m_free_blocks[0] = capacity;
        }
    }

    /**
     * allocates a block of memory of a specified size.
     * @param size the size of the memory to allocate in bytes.
     * @return an `allocationhandle` to the allocated block.
     */
    AllocationHandle allocate(size_t size)
    {
        if (size == 0) {
            return AllocationHandle();
        }

        const size_t alignment  = alignof(T);
        const size_t total_size = (size + alignment - 1) & ~(alignment - 1);

        for (auto it = m_free_blocks.begin(); it != m_free_blocks.end(); ++it) {
            if (it->second >= total_size) {
                size_t free_offset = it->first;
                size_t free_size   = it->second;

                m_free_blocks.erase(it);

                size_t remaining_size = free_size - total_size;
                if (remaining_size > 0) {
                    m_free_blocks[free_offset + total_size] = remaining_size;
                }

                m_allocated_blocks[free_offset] = total_size;

                return AllocationHandle(free_offset);
            }
        }

        return AllocationHandle(); // not enough memory
    }

    /**
     * deallocates a previously allocated block of memory.
     * @param handle the handle to the block to deallocate.
     */
    void deallocate(AllocationHandle handle)
    {
        if (!handle.valid()) return;

        auto it = m_allocated_blocks.find(handle.offset);

        // trying to free something not allocated by us, or double free
        if (it == m_allocated_blocks.end()) return;

        size_t block_offset = it->first;
        size_t block_size   = it->second;
        m_allocated_blocks.erase(it);

        // add to free list and coalesce
        size_t new_free_offset = block_offset;
        size_t new_free_size   = block_size;

        // coalesce with next block
        auto next_it = m_free_blocks.find(new_free_offset + new_free_size);
        if (next_it != m_free_blocks.end()) {
            new_free_size += next_it->second;
            m_free_blocks.erase(next_it);
        }

        // coalesce with previous block
        auto prev_it = m_free_blocks.lower_bound(new_free_offset);
        if (prev_it != m_free_blocks.begin()) {
            --prev_it;
            if (prev_it->first + prev_it->second == new_free_offset) {
                new_free_offset = prev_it->first;
                new_free_size += prev_it->second;
                m_free_blocks.erase(prev_it);
            }
        }

        m_free_blocks[new_free_offset] = new_free_size;
    }

    /**
     * resets the allocator, freeing all allocated blocks and making the entire capacity available again.
     */
    void reset()
    {
        m_free_blocks.clear();
        m_allocated_blocks.clear();
        if (m_capacity > 0) {
            m_free_blocks[0] = m_capacity;
        }
    }
};

#endif // LYRA_PLUGIN_D3D12_BLOCK_ALLOCATOR_H
