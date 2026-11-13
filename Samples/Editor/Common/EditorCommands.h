#pragma once

#ifndef LYRA_EDITOR_COMMON_EDITOR_COMMANDS_H
#define LYRA_EDITOR_COMMON_EDITOR_COMMANDS_H

namespace lyra
{
    struct AppContext;

    // registers all built-in editor commands to the command palette and shortcut dispatcher
    void setup_editor_commands(AppContext& context);

    // renders modal dialogs triggered by editor commands (e.g. new scene dialog)
    void render_editor_modals(AppContext& context);

} // namespace lyra

#endif // LYRA_EDITOR_COMMON_EDITOR_COMMANDS_H
