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

    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);
    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content);

    // terminal leaf node (non-expandable)
    void tree_leaf(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select);
    void tree_leaf(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H
