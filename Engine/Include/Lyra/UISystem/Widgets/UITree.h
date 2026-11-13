#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H

#include <Lyra/Utilities/String.h>
#include <Lyra/UISystem/Widgets/UI.h>

namespace lyra::ui
{
    // hierarchical expandable tree node with icon, label, and selection state.
    // 'content' is invoked only when the item is expanded.
    bool tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select);
    bool tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content);
    bool tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef on_header, ActionRef content);

    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);
    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content);
    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef on_header, ActionRef content);

    // terminal leaf node (non-expandable)
    void tree_leaf(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select);
    void tree_leaf(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);

    // set expansion state for the next tree node
    void tree_set_next_open(bool is_open);

    // calculates layout width required to display a tree node (arrow + icon + label text + padding + scrollbar)
    auto calc_tree_item_width(CString label, CString icon = nullptr, bool reserve_scrollbar = true) -> float;

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H
