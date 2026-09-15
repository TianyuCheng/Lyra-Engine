#pragma once

#ifndef LYRA_LYRA_UICORE_UITREE_H
#define LYRA_LYRA_UICORE_UITREE_H

#include <Lyra/Common/String.h>
#include <Lyra/UICore/UI.h>

namespace lyra::ui
{
    // Hierarchical expandable tree node with icon, label, and selection state.
    // 'content' is invoked only when the item is expanded.
    bool tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select);
    bool tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content);

    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);
    bool tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content);

    // Terminal leaf node (non-expandable)
    void tree_leaf(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select);
    void tree_leaf(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select);

} // namespace lyra::ui

#endif // LYRA_LYRA_UICORE_UITREE_H
