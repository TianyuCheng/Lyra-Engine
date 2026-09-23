#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H

#include <Lyra/Utilities/String.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui::workspace
{
    struct LayoutSplit
    {
        float left   = 0.25f;
        float right  = 0.25f;
        float top    = 0.25f;
        float bottom = 0.25f;
    };

    struct LayoutNodes
    {
        uint32_t main   = 0;
        uint32_t left   = 0;
        uint32_t right  = 0;
        uint32_t top    = 0;
        uint32_t bottom = 0;
    };

    // initialize/configure the editor workspace layout splits
    void setup(const LayoutSplit& split = {});

    // dock a panel to a specific area of the workspace
    void dock(CString panel_title, Area area);

    // get the IDs of the split regions
    auto get_nodes() -> LayoutNodes;

} // namespace lyra::ui::workspace

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UIDOCK_H
