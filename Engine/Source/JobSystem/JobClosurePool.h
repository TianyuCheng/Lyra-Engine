#pragma once

#ifndef LYRA_ENGINE_JOBSYSTEM_CLOSURE_POOL_H
#define LYRA_ENGINE_JOBSYSTEM_CLOSURE_POOL_H

#include <mutex>
#include <cstdlib>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>

namespace lyra
{
    constexpr size_t CLOSURE_BLOCK_SIZE = 128;

    struct alignas(16) ClosureBlock
    {
        ClosureBlock* next = nullptr;
    };

    struct CentralClosurePool
    {
        std::mutex    mutex;
        Vector<void*> pages;
        ClosureBlock* free_list = nullptr;

        void* allocate()
        {
            std::lock_guard lock(mutex);
            if (!free_list) {
                constexpr size_t PAGE_SIZE   = 64 * 1024;
                constexpr size_t BLOCK_COUNT = PAGE_SIZE / CLOSURE_BLOCK_SIZE;

                auto page = static_cast<uint8_t*>(std::malloc(PAGE_SIZE));
                pages.push_back(page);

                for (size_t i = 0; i < BLOCK_COUNT; ++i) {
                    auto block  = reinterpret_cast<ClosureBlock*>(page + i * CLOSURE_BLOCK_SIZE);
                    block->next = free_list;
                    free_list   = block;
                }
            }

            auto block = free_list;
            free_list  = free_list->next;
            return block;
        }

        void deallocate(void* ptr)
        {
            std::lock_guard lock(mutex);
            auto            block = static_cast<ClosureBlock*>(ptr);
            block->next           = free_list;
            free_list             = block;
        }

        void clear()
        {
            std::lock_guard lock(mutex);
            for (void* page : pages) {
                std::free(page);
            }
            pages.clear();
            free_list = nullptr;
        }
    };

    struct ThreadClosureCache
    {
        static constexpr size_t MAX_CACHED = 64;
        ClosureBlock*           free_list  = nullptr;
        size_t                  count      = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_JOBSYSTEM_CLOSURE_POOL_H
