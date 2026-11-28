#include <chrono>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>

#include <Lyra/Utilities/Macros.h>
#include <Lyra/Graphics/RHIError.h>
#include "RHIDiagnosticOverlay.h"

using namespace lyra;

namespace lyra::rhi::diagnostics
{
    auto get_logger() -> Logger
    {
        static Logger LOGGER = create_logger("Graphics", LogLevel::info);
        return LOGGER;
    }

    auto generate_iso_timestamp() -> String
    {
        auto now       = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    auto generate_file_timestamp() -> String
    {
        auto now       = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&in_time_t), "%Y%m%d_%H%M%S");
        return ss.str();
    }

    // --- Intercepted Function Pointers for RenderAPI Overlay ---

    void overlay_cmd_draw(GPUCommandEncoderHandle cmdbuffer, GPUSize32 vertex_count, GPUSize32 instance_count, GPUSize32 first_vertex, GPUSize32 first_instance)
    {
        RHIDiagnosticOverlay::record_draw(vertex_count, instance_count, first_vertex);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_draw) {
            base->cmd_draw(cmdbuffer, vertex_count, instance_count, first_vertex, first_instance);
        }
    }

    void overlay_cmd_draw_indexed(GPUCommandEncoderHandle cmdbuffer, GPUSize32 index_count, GPUSize32 instance_count, GPUSize32 first_index, GPUSignedOffset32 base_vertex, GPUSize32 first_instance)
    {
        RHIDiagnosticOverlay::record_draw_indexed(index_count, instance_count, first_index);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_draw_indexed) {
            base->cmd_draw_indexed(cmdbuffer, index_count, instance_count, first_index, base_vertex, first_instance);
        }
    }

    void overlay_cmd_draw_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        RHIDiagnosticOverlay::record_draw(draw_count, 1, 0);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_draw_indirect) {
            base->cmd_draw_indirect(cmdbuffer, indirect_buffer, indirect_offset, draw_count);
        }
    }

    void overlay_cmd_draw_indexed_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset, GPUSize32 draw_count)
    {
        RHIDiagnosticOverlay::record_draw_indexed(draw_count, 1, 0);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_draw_indexed_indirect) {
            base->cmd_draw_indexed_indirect(cmdbuffer, indirect_buffer, indirect_offset, draw_count);
        }
    }

    void overlay_cmd_dispatch_workgroups(GPUCommandEncoderHandle cmdbuffer, GPUSize32 x, GPUSize32 y, GPUSize32 z)
    {
        RHIDiagnosticOverlay::record_dispatch(x, y, z);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_dispatch_workgroups) {
            base->cmd_dispatch_workgroups(cmdbuffer, x, y, z);
        }
    }

    void overlay_cmd_dispatch_workgroups_indirect(GPUCommandEncoderHandle cmdbuffer, GPUBufferHandle indirect_buffer, GPUSize64 indirect_offset)
    {
        RHIDiagnosticOverlay::record_dispatch(1, 1, 1);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_dispatch_workgroups_indirect) {
            base->cmd_dispatch_workgroups_indirect(cmdbuffer, indirect_buffer, indirect_offset);
        }
    }

    void overlay_cmd_push_debug_group(GPUCommandEncoderHandle cmdbuffer, CString group_label)
    {
        if (group_label) {
            RHIDiagnosticOverlay::record_pass_begin(group_label);
        }
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_push_debug_group) {
            base->cmd_push_debug_group(cmdbuffer, group_label);
        }
    }

    void overlay_cmd_pop_debug_group(GPUCommandEncoderHandle cmdbuffer)
    {
        RHIDiagnosticOverlay::record_pass_end();
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_pop_debug_group) {
            base->cmd_pop_debug_group(cmdbuffer);
        }
    }

    void overlay_cmd_insert_debug_marker(GPUCommandEncoderHandle cmdbuffer, CString marker_label)
    {
        if (marker_label) {
            RHIDiagnosticOverlay::record_marker(marker_label);
        }
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_insert_debug_marker) {
            base->cmd_insert_debug_marker(cmdbuffer, marker_label);
        }
    }

    void overlay_cmd_set_render_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURenderPipelineHandle pipeline)
    {
        RHIDiagnosticOverlay::record_pipeline(pipeline.value);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_set_render_pipeline) {
            base->cmd_set_render_pipeline(cmdbuffer, pipeline);
        }
    }

    void overlay_cmd_set_compute_pipeline(GPUCommandEncoderHandle cmdbuffer, GPUComputePipelineHandle pipeline)
    {
        RHIDiagnosticOverlay::record_pipeline(pipeline.value);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_set_compute_pipeline) {
            base->cmd_set_compute_pipeline(cmdbuffer, pipeline);
        }
    }

    void overlay_cmd_set_raytracing_pipeline(GPUCommandEncoderHandle cmdbuffer, GPURayTracingPipelineHandle pipeline)
    {
        RHIDiagnosticOverlay::record_pipeline(pipeline.value);
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (base && base->cmd_set_raytracing_pipeline) {
            base->cmd_set_raytracing_pipeline(cmdbuffer, pipeline);
        }
    }

    bool overlay_submit_command_buffer(GPUCommandEncoderHandle cmdbuffer)
    {
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (!base || !base->submit_command_buffer) return false;

        try {
            bool ok = base->submit_command_buffer(cmdbuffer);
            if (!ok) {
                RHIDiagnosticOverlay::on_device_loss("submit_command_buffer returned false", "", false);
            }
            return ok;
        } catch (const GPUDeviceLostInfo& e) {
            RHIDiagnosticOverlay::on_device_loss(e.what(), "", false);
            throw;
        } catch (const std::exception& e) {
            RHIDiagnosticOverlay::on_device_loss(e.what(), "", false);
            throw;
        }
    }

    void overlay_wait_idle()
    {
        auto base = RHIDiagnosticOverlay::get_base_api();
        if (!base || !base->wait_idle) return;

        try {
            base->wait_idle();
        } catch (const GPUDeviceLostInfo& e) {
            RHIDiagnosticOverlay::on_device_loss(e.what(), "", false);
            throw;
        } catch (const std::exception& e) {
            RHIDiagnosticOverlay::on_device_loss(e.what(), "", false);
            throw;
        }
    }

} // namespace lyra::rhi::diagnostics

using namespace lyra::rhi::diagnostics;

RenderAPI* RHIDiagnosticOverlay::init(RenderAPI* base_api, StringView backend_name)
{
    LockGuard lock(s_mutex);

    s_base_api     = base_api;
    s_backend_name = backend_name.empty() ? "Generic" : String(backend_name);
    s_capacity     = DEFAULT_RING_CAPACITY;
    s_entries.assign(s_capacity, RHIBreadcrumbEntry{});
    s_total_recorded       = 0;
    s_active_pass_hash     = 0;
    s_active_pass_name[0]  = '\0';
    s_active_pipeline_hash = 0;

    // copy base table
    s_intercepted_api = *base_api;

    // intercept drawing & execution
    s_intercepted_api.cmd_draw                         = overlay_cmd_draw;
    s_intercepted_api.cmd_draw_indexed                 = overlay_cmd_draw_indexed;
    s_intercepted_api.cmd_draw_indirect                = overlay_cmd_draw_indirect;
    s_intercepted_api.cmd_draw_indexed_indirect        = overlay_cmd_draw_indexed_indirect;
    s_intercepted_api.cmd_dispatch_workgroups          = overlay_cmd_dispatch_workgroups;
    s_intercepted_api.cmd_dispatch_workgroups_indirect = overlay_cmd_dispatch_workgroups_indirect;

    // intercept debug annotations
    s_intercepted_api.cmd_push_debug_group    = overlay_cmd_push_debug_group;
    s_intercepted_api.cmd_pop_debug_group     = overlay_cmd_pop_debug_group;
    s_intercepted_api.cmd_insert_debug_marker = overlay_cmd_insert_debug_marker;

    // intercept pipeline state
    s_intercepted_api.cmd_set_render_pipeline     = overlay_cmd_set_render_pipeline;
    s_intercepted_api.cmd_set_compute_pipeline    = overlay_cmd_set_compute_pipeline;
    s_intercepted_api.cmd_set_raytracing_pipeline = overlay_cmd_set_raytracing_pipeline;

    // intercept device execution and completion
    s_intercepted_api.submit_command_buffer = overlay_submit_command_buffer;
    s_intercepted_api.wait_idle             = overlay_wait_idle;

    s_enabled = true;
    return &s_intercepted_api;
}

void RHIDiagnosticOverlay::shutdown()
{
    LockGuard lock(s_mutex);
    s_base_api = nullptr;
    s_enabled  = false;
    s_entries.clear();
    s_total_recorded = 0;
}

bool RHIDiagnosticOverlay::is_enabled()
{
    return s_enabled;
}

View<RHIBreadcrumbEntry> RHIDiagnosticOverlay::get_entries()
{
    LockGuard lock(s_mutex);
    return View<RHIBreadcrumbEntry>(s_entries.data(), s_capacity);
}

uint RHIDiagnosticOverlay::get_total_recorded()
{
    LockGuard lock(s_mutex);
    return s_total_recorded;
}

uint RHIDiagnosticOverlay::push_entry(const RHIBreadcrumbEntry& entry)
{
    uint seq            = ++s_total_recorded;
    uint slot           = (seq - 1) % s_capacity;
    auto copy           = entry;
    copy.sequence_index = seq;

    if (slot < s_entries.size()) {
        s_entries[slot] = copy;
    }
    return seq;
}

uint RHIDiagnosticOverlay::record_pass_begin(StringView pass_name, ulong pass_hash)
{
    LockGuard lock(s_mutex);

    if (pass_hash == 0) {
        pass_hash = deterministic_guid(0, pass_name);
    }
    s_active_pass_hash = pass_hash;

    usize len = std::min(pass_name.size(), sizeof(s_active_pass_name) - 1);
    std::memcpy(s_active_pass_name, pass_name.data(), len);
    s_active_pass_name[len] = '\0';

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::PASS_BEGIN);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));

    return push_entry(entry);
}

uint RHIDiagnosticOverlay::record_pass_end()
{
    LockGuard lock(s_mutex);

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::PASS_END);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));

    uint seq = push_entry(entry);

    s_active_pass_hash    = 0;
    s_active_pass_name[0] = '\0';

    return seq;
}

void RHIDiagnosticOverlay::record_pipeline(ulong pipeline_hash)
{
    LockGuard lock(s_mutex);

    s_active_pipeline_hash = pipeline_hash;

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::PIPELINE_BIND);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));

    push_entry(entry);
}

uint RHIDiagnosticOverlay::record_draw(uint vertex_count, uint instance_count, uint first_vertex)
{
    LockGuard lock(s_mutex);

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::DRAW);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));
    entry.arg0 = vertex_count;
    entry.arg1 = instance_count;
    entry.arg2 = first_vertex;

    return push_entry(entry);
}

uint RHIDiagnosticOverlay::record_draw_indexed(uint index_count, uint instance_count, uint first_index)
{
    LockGuard lock(s_mutex);

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::DRAW_INDEXED);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));
    entry.arg0 = index_count;
    entry.arg1 = instance_count;
    entry.arg2 = first_index;

    return push_entry(entry);
}

uint RHIDiagnosticOverlay::record_dispatch(uint x, uint y, uint z)
{
    LockGuard lock(s_mutex);

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::DISPATCH);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));
    entry.arg0 = x;
    entry.arg1 = y;
    entry.arg2 = z;

    return push_entry(entry);
}

uint RHIDiagnosticOverlay::record_marker(StringView marker_label)
{
    LockGuard lock(s_mutex);

    RHIBreadcrumbEntry entry{};
    entry.op            = static_cast<uint>(RHIBreadcrumbOp::DEBUG_MARKER);
    entry.pass_hash     = s_active_pass_hash;
    entry.pipeline_hash = s_active_pipeline_hash;
    std::memcpy(entry.pass_name, s_active_pass_name, sizeof(entry.pass_name));

    usize len = std::min(marker_label.size(), sizeof(entry.marker_name) - 1);
    std::memcpy(entry.marker_name, marker_label.data(), len);
    entry.marker_name[len] = '\0';

    return push_entry(entry);
}

void RHIDiagnosticOverlay::mark_completed(uint sequence_index)
{
    LockGuard lock(s_mutex);
    if (sequence_index == 0 || sequence_index > s_total_recorded) return;

    uint slot = (sequence_index - 1) % s_capacity;
    if (slot < s_entries.size() && s_entries[slot].sequence_index == sequence_index) {
        s_entries[slot].completed = 1;
    }
}

void RHIDiagnosticOverlay::mark_all_completed_up_to(uint sequence_index)
{
    LockGuard lock(s_mutex);
    if (sequence_index == 0) return;

    uint start_seq = (s_total_recorded > s_capacity) ? (s_total_recorded - s_capacity + 1) : 1;
    for (uint seq = start_seq; seq <= sequence_index && seq <= s_total_recorded; ++seq) {
        uint slot = (seq - 1) % s_capacity;
        if (slot < s_entries.size() && s_entries[slot].sequence_index == seq) {
            s_entries[slot].completed = 1;
        }
    }
}

void RHIDiagnosticOverlay::reset_frame()
{
    LockGuard lock(s_mutex);
    s_active_pass_hash     = 0;
    s_active_pass_name[0]  = '\0';
    s_active_pipeline_hash = 0;
}

RHICrashReport RHIDiagnosticOverlay::analyze(StringView failure_reason, StringView backend)
{
    LockGuard lock(s_mutex);

    RHICrashReport report;
    report.timestamp      = generate_iso_timestamp();
    report.backend_name   = backend.empty() ? s_backend_name : String(backend);
    report.failure_reason = failure_reason.empty() ? "GPU device loss / hang detected" : String(failure_reason);
    report.total_commands = s_total_recorded;

    if (s_total_recorded == 0 || s_entries.empty()) {
        return report;
    }

    uint window_count = std::min(s_total_recorded, s_capacity);
    uint start_seq    = (s_total_recorded > s_capacity) ? (s_total_recorded - s_capacity + 1) : 1;

    const RHIBreadcrumbEntry* hung_entry = nullptr;
    for (isize seq = static_cast<isize>(s_total_recorded); seq >= static_cast<isize>(start_seq); --seq) {
        uint slot = static_cast<uint>((seq - 1) % s_capacity);
        if (slot < s_entries.size() && s_entries[slot].sequence_index == static_cast<uint>(seq) && s_entries[slot].completed == 0) {
            hung_entry = &s_entries[slot];
            break;
        }
    }

    if (!hung_entry) {
        uint last_slot = (s_total_recorded - 1) % s_capacity;
        hung_entry     = &s_entries[last_slot];
    }

    report.hung_sequence = hung_entry->sequence_index;
    report.hung_entry    = *hung_entry;

    // collect recent history in chronological order
    constexpr uint MAX_HISTORY       = 32;
    uint           history_count     = std::min(window_count, MAX_HISTORY);
    uint           history_start_seq = (s_total_recorded >= history_count) ? (s_total_recorded - history_count + 1) : 1;

    report.recent_history.reserve(history_count);
    for (uint seq = history_start_seq; seq <= s_total_recorded; ++seq) {
        uint slot = (seq - 1) % s_capacity;
        if (slot < s_entries.size() && s_entries[slot].sequence_index == seq) {
            report.recent_history.push_back(s_entries[slot]);
        }
    }

    return report;
}

String RHIDiagnosticOverlay::format_report(const RHICrashReport& report)
{
    std::stringstream ss;
    ss << "================================================================================\n";
    ss << "                    LYRA GPU CRASH & BREADCRUMB DUMP\n";
    ss << "================================================================================\n";
    ss << "Timestamp:       " << report.timestamp << "\n";
    ss << "Backend:         " << report.backend_name << "\n";
    ss << "Failure Reason:  " << report.failure_reason << "\n";
    ss << "Total Commands:  " << report.total_commands << "\n";
    ss << "Hung Sequence:   #" << report.hung_sequence << "\n";
    ss << "\n";
    ss << "[ROOT CAUSE DIAGNOSIS]\n";
    ss << "--------------------------------------------------------------------------------\n";
    ss << "Failed Pass:     " << (report.hung_entry.pass_name[0] != '\0' ? report.hung_entry.pass_name : "<None>")
       << " (Hash: 0x" << std::hex << std::uppercase << report.hung_entry.pass_hash << std::dec << ")\n";
    ss << "Operation:       " << to_string(static_cast<RHIBreadcrumbOp>(report.hung_entry.op)) << "\n";

    auto op = static_cast<RHIBreadcrumbOp>(report.hung_entry.op);
    if (op == RHIBreadcrumbOp::DRAW || op == RHIBreadcrumbOp::DRAW_INDEXED) {
        ss << "Draw Geometry:   count=" << report.hung_entry.arg0
           << ", instances=" << report.hung_entry.arg1
           << ", start=" << report.hung_entry.arg2 << "\n";
    } else if (op == RHIBreadcrumbOp::DISPATCH) {
        ss << "Workgroups:      (" << report.hung_entry.arg0 << ", "
           << report.hung_entry.arg1 << ", " << report.hung_entry.arg2 << ")\n";
    }

    if (report.hung_entry.marker_name[0] != '\0') {
        ss << "Marker Label:    " << report.hung_entry.marker_name << "\n";
    }
    ss << "Pipeline Hash:   0x" << std::hex << std::uppercase << report.hung_entry.pipeline_hash << std::dec << "\n";
    ss << "\n";

    ss << "[RECENT EXECUTION HISTORY (CHRONOLOGICAL)]\n";
    ss << "--------------------------------------------------------------------------------\n";
    for (const auto& entry : report.recent_history) {
        bool is_hung = (entry.sequence_index == report.hung_sequence);
        ss << " #" << std::setw(5) << std::left << entry.sequence_index
           << " [" << std::setw(15) << to_string(static_cast<RHIBreadcrumbOp>(entry.op)) << "] "
           << std::setw(20) << (entry.pass_name[0] != '\0' ? entry.pass_name : "<None>");

        if (entry.op == static_cast<uint>(RHIBreadcrumbOp::DISPATCH)) {
            ss << " | dispatch(" << entry.arg0 << "," << entry.arg1 << "," << entry.arg2 << ")";
        } else if (entry.op == static_cast<uint>(RHIBreadcrumbOp::DRAW) ||
                   entry.op == static_cast<uint>(RHIBreadcrumbOp::DRAW_INDEXED)) {
            ss << " | count=" << entry.arg0;
        } else if (entry.op == static_cast<uint>(RHIBreadcrumbOp::DEBUG_MARKER)) {
            ss << " | " << entry.marker_name;
        }

        if (is_hung) {
            ss << " <-- [HUNG / DEVICE LOSS]";
        } else if (entry.completed) {
            ss << " (ok)";
        }
        ss << "\n";
    }
    ss << "================================================================================\n";

    return ss.str();
}

Path RHIDiagnosticOverlay::write_crash_dump(const RHICrashReport& report, const Path& output_dir)
{
    String report_text = format_report(report);
    get_logger()->error("{}", report_text);

    String filename  = "crash_gpu_" + generate_file_timestamp() + ".log";
    Path   full_path = output_dir / filename;

    std::ofstream out(full_path, std::ios::out | std::ios::trunc);
    if (out.is_open()) {
        out << report_text;
        out.close();
        get_logger()->info("GPU crash diagnostics successfully written to {}", full_path.string());
    } else {
        get_logger()->error("Failed to open GPU crash dump file at {}", full_path.string());
    }

    return full_path;
}

Path RHIDiagnosticOverlay::on_device_loss(StringView failure_reason, StringView backend, bool trigger_break)
{
    auto report    = analyze(failure_reason, backend);
    auto dump_path = write_crash_dump(report);

    if (trigger_break) {
        DEBUG_BREAK();
    }
    return dump_path;
}
