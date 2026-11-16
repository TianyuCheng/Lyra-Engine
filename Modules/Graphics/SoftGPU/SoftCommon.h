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

    template <typename TResource>
    inline auto fetch_resource(Slotmap<TResource>& pool, auto handle) -> TResource&
    {
        auto  slot_handle = handle.template to_slotmap_handle<TResource>();
        auto* ptr         = pool.find(slot_handle);
        if (!ptr) {
            get_logger()->error("Failed to fetch software RHI resource handle: {}", handle.value);
            std::abort();
        }
        return *ptr;
    }
} // namespace lyra::softgpu

#endif // LYRA_MODULES_GRAPHICS_SOFTGPU_SOFTCOMMON_H
