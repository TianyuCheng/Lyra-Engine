#pragma once

#ifndef LYRA_LIBRARY_EDITOR_SELECTION_MODEL_H
#define LYRA_LIBRARY_EDITOR_SELECTION_MODEL_H

#include <Lyra/Common/String.h>
#include <Lyra/Common/Collections.h>

namespace lyra
{
    /**
     * @brief Manages multi-selection logic for strings (e.g. file names, node names).
     */
    struct SelectionModel
    {
    public:
        Vector<String> items;         ///< List of currently selected item names.
        String         last_selected; ///< Anchor for range selection.

        /**
         * @brief Check if an item is currently selected.
         */
        bool is_selected(StringView item) const;

        /**
         * @brief Toggle the selection state of an item (Ctrl/Cmd click behavior).
         */
        void toggle(StringView item);

        /**
         * @brief Clear existing selection and select only the specified item.
         */
        void select_only(StringView item);

        /**
         * @brief Select a range of items from last_selected to the target item (Shift click behavior).
         * @param item The target item.
         * @param all_items The full list of items in the current view to determine the range.
         */
        void select_range(StringView item, const Vector<String>& all_items);

        /**
         * @brief Clear all selected items.
         */
        void clear();

        /**
         * @brief Check if the selection is empty.
         */
        bool empty() const { return items.empty(); }

        /**
         * @brief Get the number of selected items.
         */
        size_t size() const { return items.size(); }
    };

} // namespace lyra

#endif // LYRA_LIBRARY_EDITOR_SELECTION_MODEL_H
