#pragma once

#ifndef LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMON_H
#define LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMON_H

#include <memory>

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Sync.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Pointer.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIError.h>
#include <Lyra/Graphics/RHIUtils.h>

namespace lyra::softgpu
{
    inline auto get_logger() -> Logger
    {
        auto l = spdlog::get("SoftGPU");
        if (!l) {
            l = create_logger("SoftGPU", LogLevel::info);
        }
        return l;
    }

    template <typename T>
    struct SoftDestroyer
    {
        void operator()(T* obj)
        {
            obj->destroy();
        }
    };

    template <typename T>
    using SoftResourceManager = Slotmap<T, SoftDestroyer<T>>;

    template <typename TResource, typename Deleter, typename Handle>
    inline auto fetch_resource(apus::slot_map<TResource, Deleter>& pool, Handle handle) -> TResource&
    {
        if (!handle.valid()) {
            get_logger()->error("Resource handle {} is invalid!", Handle::type_name());
            throw GPUInternalError("Resource handle is invalid!");
        }
        auto slot_handle = handle.template to_slotmap_handle<TResource>();
        auto ptr         = pool.find(slot_handle);
        if (!ptr) {
            get_logger()->error("Failed to fetch software RHI resource handle {}: {}", Handle::type_name(), handle.value);
            throw GPUInternalError("Software RHI resource handle references cannot be found!");
        }
        return *ptr;
    }

    template <typename TResource, typename Deleter, typename Handle>
    inline auto fetch_resource(const apus::slot_map<TResource, Deleter>& pool, Handle handle) -> const TResource&
    {
        if (!handle.valid()) {
            get_logger()->error("Resource handle {} is invalid!", Handle::type_name());
            throw GPUInternalError("Resource handle is invalid!");
        }
        auto       slot_handle = handle.template to_slotmap_handle<TResource>();
        const auto ptr         = pool.find(slot_handle);
        if (!ptr) {
            get_logger()->error("Failed to fetch software RHI resource handle {}: {}", Handle::type_name(), handle.value);
            throw GPUInternalError("Software RHI resource handle references cannot be found!");
        }
        return *ptr;
    }
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMON_H
