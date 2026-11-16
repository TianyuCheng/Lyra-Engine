#pragma once

#ifndef LYRA_EDITOR_COMMON_EDITOR_MENU_BAR_H
#define LYRA_EDITOR_COMMON_EDITOR_MENU_BAR_H

namespace lyra
{
    struct AppContext;

    // renders the main editor menu bar (scene, assets, window, etc.)
    void render_editor_menubar(AppContext& context);

} // namespace lyra

#endif // LYRA_EDITOR_COMMON_EDITOR_MENU_BAR_H
