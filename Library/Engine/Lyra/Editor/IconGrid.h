#pragma once

#ifndef LYRA_LIBRARY_EDITOR_ICON_GRID_H
#define LYRA_LIBRARY_EDITOR_ICON_GRID_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Stdint.h>
#include <Lyra/Common/GUI.h>

namespace lyra
{
    /**
     * @brief A reusable component for rendering a grid of interactive icons.
     */
    struct IconGrid
    {
    public:
        float icon_size  = 128.0f;
        float icon_scale = 6.0f;
        float padding    = 16.0f;

        /**
         * @brief Interaction flags returned by draw_item.
         */
        enum InteractionFlags : int
        {
            None           = 0,
            Clicked        = 1 << 0,
            DoubleClicked  = 1 << 1,
            RightClicked   = 1 << 2,
            Hovered        = 1 << 3
        };

        struct Context
        {
            float start_x;
            float row_width;
        };

        /**
         * @brief Initialize the grid context for the current window.
         */
        auto begin() -> Context;

        /**
         * @brief Draw a single item in the grid and handle its layout.
         * @return Bitmask of InteractionFlags.
         */
        auto draw_item(Context& ctx, CString icon, CString label, bool selected) -> int;

    private:
        void next_column(Context& ctx);
    };

} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_ICON_GRID_H
