#include <Lyra/Graphics/RHIDiagnostics.h>
#include "RHIDiagnosticOverlay.h"

using namespace lyra;

bool RHIDiagnostics::is_enabled()
{
    return RHIDiagnosticOverlay::is_enabled();
}

Span<const RHIBreadcrumbEntry> RHIDiagnostics::get_entries()
{
    return RHIDiagnosticOverlay::get_entries();
}

uint RHIDiagnostics::get_total_recorded()
{
    return RHIDiagnosticOverlay::get_total_recorded();
}

RHICrashReport RHIDiagnostics::analyze(StringView failure_reason, StringView backend)
{
    return RHIDiagnosticOverlay::analyze(failure_reason, backend);
}

String RHIDiagnostics::format_report(const RHICrashReport& report)
{
    return RHIDiagnosticOverlay::format_report(report);
}

Path RHIDiagnostics::write_crash_dump(const RHICrashReport& report, const Path& output_dir)
{
    return RHIDiagnosticOverlay::write_crash_dump(report, output_dir);
}

Path RHIDiagnostics::on_device_loss(StringView failure_reason, StringView backend, bool trigger_break)
{
    return RHIDiagnosticOverlay::on_device_loss(failure_reason, backend, trigger_break);
}

void RHIDiagnostics::reset_frame()
{
    RHIDiagnosticOverlay::reset_frame();
}
