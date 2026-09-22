#pragma once

#ifndef LYRA_LYRA_UICORE_UILAYOUT_H
#define LYRA_LYRA_UICORE_UILAYOUT_H

#include <Lyra/Common/Math.h>
#include <Lyra/UICore/UI.h>

namespace lyra::ui
{
    struct RowDescriptor
    {
        Alignment align    = Alignment::Start;
        VAlign    vertical = VAlign::Center;
        float     spacing  = -1.0f;
    };

    // directional flow
    void row(ActionRef content);
    void row(Alignment align, ActionRef content);
    void row(Alignment align, VAlign vertical, ActionRef content);
    void row(const RowDescriptor& desc, ActionRef content);
    void column(ActionRef content);
    void align_text_to_frame_padding();

    // standardized toolbar (action buttons, level toggles, search)
    void toolbar(ActionRef content);

    // scrollable container for logs, file browsers, etc.
    void scroll_area(CString id, ActionRef content);
    void scroll_area(CString id, float reserve_bottom, ActionRef content);
    void scroll_to_bottom();

    // dynamic flexible spacer (spring): pushes subsequent items in a row to the far right
    void spacer();

    // responsive grid layout: automatically arranges items in columns based on cell width
    void grid(CString id, float item_width, ActionRef content);
    void grid_item(ActionRef content);

    // semantic divider between items or sections
    void separator();

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UILAYOUT_H
