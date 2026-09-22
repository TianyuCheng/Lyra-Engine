#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_UIENUMS_H
#define LYRA_ENGINE_UISYSTEM_UIENUMS_H

#include <Lyra/Utilities/Stdint.h>

namespace lyra::ui
{
    // semantic button roles (dictates uniform styling across the engine)
    enum struct ButtonRole : uint8_t
    {
        Standard,
        Primary,    // Highlighted / Accent action (e.g. Save, Create, Active State)
        Success,    // Positive / Play action (e.g. Play, Run)
        Warning,    // Cautionary action (e.g. Pause)
        Danger,     // Destructive action (e.g. Delete, Purge)
        Ghost       // Borderless / Minimal (e.g. toolbars, breadcrumbs)
    };

    // semantic status roles for text, badges, and filters
    enum struct StatusRole : uint8_t
    {
        Default,
        Muted,
        Trace,
        Debug,
        Info,
        Success,
        Warning,
        Error,
        Critical
    };

    // directional alignment along layout axes
    enum struct Alignment : uint8_t
    {
        Start,
        Center,
        End
    };

    // vertical alignment along cross-layout axes
    enum struct VAlign : uint8_t
    {
        Top,
        Center,
        Bottom,
        Baseline
    };

    // docking workspace areas
    enum struct Area : uint8_t
    {
        Main,
        Left,
        Right,
        Top,
        Bottom
    };

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_UIENUMS_H
