#pragma once

#ifndef LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTICS_H
#define LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTICS_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Path.h>

namespace lyra
{
    /**
     * @brief Low-level RHI command operations recorded by the diagnostic overlay.
     */
    enum class RHIBreadcrumbOp : uint
    {
        PASS_BEGIN = 0,
        PASS_END,
        DRAW,
        DRAW_INDEXED,
        DRAW_INDIRECT,
        DRAW_INDEXED_INDIRECT,
        DISPATCH,
        DISPATCH_INDIRECT,
        PIPELINE_BIND,
        DEBUG_MARKER,
    };

    /**
     * @brief Pod breadcrumb entry captured in the diagnostic overlay ring buffer.
     */
    struct alignas(16) RHIBreadcrumbEntry
    {
        uint  sequence_index  = 0;
        uint  op              = 0; // RHIBreadcrumbOp
        ulong pass_hash       = 0;
        ulong pipeline_hash   = 0;
        char  pass_name[32]   = {};
        char  marker_name[32] = {};
        uint  arg0            = 0; // vertex_count / workgroup_x / draw_count
        uint  arg1            = 0; // instance_count / workgroup_y
        uint  arg2            = 0; // first_vertex / first_index / workgroup_z
        uint  completed       = 0; // 0 = in-flight, 1 = completed
    };

    /**
     * @brief Structured diagnostic crash report generated on device removal or hang.
     */
    struct RHICrashReport
    {
        String                     timestamp;
        String                     backend_name;
        String                     failure_reason;
        uint                       total_commands = 0;
        uint                       hung_sequence  = 0;
        RHIBreadcrumbEntry         hung_entry     = {};
        Vector<RHIBreadcrumbEntry> recent_history;
    };

    /**
     * @brief RHI Diagnostic and crash root-cause analysis services.
     */
    class RHIDiagnostics
    {
    public:
        static constexpr uint DEFAULT_RING_CAPACITY = 4096;

        /**
         * @brief Check whether the RHI diagnostic overlay is active.
         */
        static auto is_enabled() -> bool;

        /**
         * @brief Get immutable span of recorded breadcrumb entries in the ring buffer.
         */
        static auto get_entries() -> Span<const RHIBreadcrumbEntry>;

        /**
         * @brief Total number of RHI commands recorded since initialization.
         */
        static auto get_total_recorded() -> uint;

        /**
         * @brief Analyze the current ring buffer state to isolate the hung command.
         */
        static auto analyze(StringView failure_reason = "", StringView backend = "") -> RHICrashReport;

        /**
         * @brief Format a human-readable diagnostic crash dump string.
         */
        static auto format_report(const RHICrashReport& report) -> String;

        /**
         * @brief Write crash diagnostics to crash_gpu_<timestamp>.log.
         * @return Full path of the generated log file.
         */
        static auto write_crash_dump(const RHICrashReport& report, const Path& output_dir = ".") -> Path;

        /**
         * @brief Invoked upon device loss: produces crash dump and optional break.
         */
        static auto on_device_loss(StringView failure_reason, StringView backend = "", bool trigger_break = true) -> Path;

        /**
         * @brief Reset frame sequence counters at swapchain frame boundaries.
         */
        static auto reset_frame() -> void;
    };

    /**
     * @brief Convert a RHIBreadcrumbOp enum value to its string representation.
     */
    inline constexpr CString to_string(RHIBreadcrumbOp op)
    {
        // clang-format off
        switch (op) {
            case RHIBreadcrumbOp::PASS_BEGIN:            return "PASS_BEGIN";
            case RHIBreadcrumbOp::PASS_END:              return "PASS_END";
            case RHIBreadcrumbOp::DRAW:                  return "DRAW";
            case RHIBreadcrumbOp::DRAW_INDEXED:          return "DRAW_INDEXED";
            case RHIBreadcrumbOp::DRAW_INDIRECT:         return "DRAW_INDIRECT";
            case RHIBreadcrumbOp::DRAW_INDEXED_INDIRECT: return "DRAW_INDEXED_INDIRECT";
            case RHIBreadcrumbOp::DISPATCH:              return "DISPATCH";
            case RHIBreadcrumbOp::DISPATCH_INDIRECT:     return "DISPATCH_INDIRECT";
            case RHIBreadcrumbOp::PIPELINE_BIND:         return "PIPELINE_BIND";
            case RHIBreadcrumbOp::DEBUG_MARKER:          return "DEBUG_MARKER";
            default:                                     return "UNKNOWN";
        }
        // clang-format on
    }

} // namespace lyra

#endif // LYRA_ENGINE_GRAPHICS_RHI_DIAGNOSTICS_H
