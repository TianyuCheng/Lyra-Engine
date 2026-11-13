#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UICOMMAND_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UICOMMAND_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/InputSystem/InputMap.h>

namespace lyra::ui
{
    /**
     * @brief descriptor for an executable command registered within the application.
     */
    struct CommandDescriptor
    {
        String                 id;                   // unique command identifier, e.g. "scene.save"
        String                 category;             // display category/group, e.g. "Scene", "Assets", "Window"
        String                 title;                // human-readable action title, e.g. "Save Scene"
        CString                icon = nullptr;       // optional glyph icon, e.g. LYRA_ICON_SAVE
        String                 shortcut;             // display shortcut text, e.g. "Ctrl+S"
        ButtonChord            chord;                // physical input chord for automatic shortcut dispatch
        Function<void() const> action;               // execution callback
        Function<bool() const> is_enabled = nullptr; // optional predicate determining if command can run
    };

    namespace command
    {
        // =====================================================================
        // 1. Command Registration & Lifecycle
        // =====================================================================

        void register_command(CommandDescriptor desc);
        void unregister_command(StringView id);
        bool execute(StringView id);
        void clear();

        auto get_command(StringView id) -> const CommandDescriptor*;
        auto get_all_commands() -> const Vector<CommandDescriptor>&;

        // =====================================================================
        // 2. Palette UI State
        // =====================================================================

        void open_palette();
        void close_palette();
        void toggle_palette();
        bool is_palette_open();

        // =====================================================================
        // 3. Per-Frame Execution
        // =====================================================================

        // dispatches global shortcuts for registered commands (no-op if text input is active)
        void dispatch_shortcuts();

        // renders the command palette overlay if active and handles Ctrl+P toggle
        void palette();

    } // namespace command

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UICOMMAND_H
