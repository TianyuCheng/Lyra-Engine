#pragma once

#ifndef LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTIC_OVERLAY_H
#define LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTIC_OVERLAY_H

#include <Lyra/Utilities/Sync.h>
#include <Lyra/Utilities/GUID.h>
#include <Lyra/Utilities/Path.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIDiagnostics.h>

namespace lyra
{
    /**
     * @brief Transparent interception layer wrapping RenderAPI for GPU crash root-cause diagnostics.
     *
     * Functions like a Vulkan loader layer: intercepts draw/dispatch/pass calls to populate
     * a persistent ring buffer of execution markers without invading backend implementations.
     */
    struct RHIDiagnosticOverlay
    {
    public:
        static constexpr uint DEFAULT_RING_CAPACITY = 4096;

        static auto init(RenderAPI* base_api, StringView backend_name = "") -> RenderAPI*;
        static auto shutdown() -> void;
        static auto is_enabled() -> bool;

        static auto get_entries() -> View<RHIBreadcrumbEntry>;
        static auto get_total_recorded() -> uint;

        static auto record_pass_begin(StringView pass_name, ulong pass_hash = 0) -> uint;
        static auto record_pass_end() -> uint;
        static auto record_pipeline(ulong pipeline_hash) -> void;
        static auto record_draw(uint vertex_count, uint instance_count, uint first_vertex = 0) -> uint;
        static auto record_draw_indexed(uint index_count, uint instance_count, uint first_index = 0) -> uint;
        static auto record_dispatch(uint x, uint y, uint z) -> uint;
        static auto record_marker(StringView marker_label) -> uint;

        static auto mark_completed(uint sequence_index) -> void;
        static auto mark_all_completed_up_to(uint sequence_index) -> void;
        static auto reset_frame() -> void;

        static auto analyze(StringView failure_reason = "", StringView backend = "") -> RHICrashReport;
        static auto format_report(const RHICrashReport& report) -> String;
        static auto write_crash_dump(const RHICrashReport& report, const Path& output_dir = ".") -> Path;
        static auto on_device_loss(StringView failure_reason, StringView backend = "", bool trigger_break = true) -> Path;

        static auto get_base_api() -> RenderAPI* { return s_base_api; }

    private:
        static auto push_entry(const RHIBreadcrumbEntry& entry) -> uint;

        static inline RenderAPI*                 s_base_api             = nullptr;
        static inline RenderAPI                  s_intercepted_api      = {};
        static inline bool                       s_enabled              = false;
        static inline String                     s_backend_name         = "Generic";
        static inline Mutex                      s_mutex                = {};
        static inline uint                       s_capacity             = DEFAULT_RING_CAPACITY;
        static inline uint                       s_total_recorded       = 0;
        static inline Vector<RHIBreadcrumbEntry> s_entries              = {};
        static inline ulong                      s_active_pass_hash     = 0;
        static inline char                       s_active_pass_name[32] = {};
        static inline ulong                      s_active_pipeline_hash = 0;
    };

} // namespace lyra

#endif // LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTIC_OVERLAY_H
