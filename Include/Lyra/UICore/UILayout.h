#pragma once

#ifndef LYRA_LYRA_UICORE_UILAYOUT_H
#define LYRA_LYRA_UICORE_UILAYOUT_H

#include <Lyra/Common/Math.h>
#include <Lyra/UICore/UI.h>

namespace lyra::ui
{
    // Directional flow
    void row(ActionRef content);
    void row(Alignment align, ActionRef content);
    void column(ActionRef content);

    // Standardized toolbar (action buttons, level toggles, search)
    void toolbar(ActionRef content);

    // Scrollable container for logs, file browsers, etc.
    void scroll_area(CString id, ActionRef content);
    void scroll_area(CString id, float reserve_bottom, ActionRef content);
    void scroll_to_bottom();

    // Dynamic flexible spacer (spring): pushes subsequent items in a row to the far right
    void spacer();

    // Responsive grid layout: automatically arranges items in columns based on cell width
    void grid(CString id, float item_width, ActionRef content);
    void grid_item(ActionRef content);

    // Semantic divider between items or sections
    void separator();

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UILAYOUT_H
