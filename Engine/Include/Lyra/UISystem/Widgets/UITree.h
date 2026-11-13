#pragma once

#ifndef LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H
#define LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/UISystem/Widgets/UI.h>
#include <Lyra/UISystem/Widgets/UIEnums.h>

namespace lyra::ui
{
    enum struct TreeNodeFlag : uint32_t
    {
        None              = 0,
        Leaf              = 1 << 0,
        DefaultOpen       = 1 << 1,
        ForceOpen         = 1 << 2,
        OpenOnDoubleClick = 1 << 3,
        Framed            = 1 << 4,
        SpanFullWidth     = 1 << 5,
    };
} // namespace lyra::ui

ENABLE_BIT_FLAGS(lyra::ui::TreeNodeFlag);

namespace lyra::ui
{
    using TreeNodeFlags = BitFlags<TreeNodeFlag>;

    struct TreeItemDescriptor
    {
        uint64_t      id          = 0;
        CString       id_str      = nullptr; // used if id == 0
        CString       label       = "";
        CString       icon        = nullptr;
        bool          is_selected = false;
        TreeNodeFlags flags       = TreeNodeFlag::SpanFullWidth;
    };

    struct TreeItemResponse
    {
        bool is_open           = false;
        bool is_clicked        = false;
        bool is_double_clicked = false;
        bool is_right_clicked  = false;
        bool is_toggled_open   = false;
        bool is_hovered        = false;

        explicit operator bool() const { return is_open; }
    };

    // primary tree node API
    auto tree_node(const TreeItemDescriptor& desc) -> TreeItemResponse;
    auto tree_node(const TreeItemDescriptor& desc, ActionRef children) -> TreeItemResponse;
    auto tree_node(const TreeItemDescriptor& desc, ActionRef on_header, ActionRef children) -> TreeItemResponse;

    // convenience overload for simple nodes / leaves by integer id
    inline auto tree_node(uint64_t id, CString label, CString icon = nullptr, bool is_selected = false, bool is_leaf = false) -> TreeItemResponse
    {
        TreeItemDescriptor desc;
        desc.id          = id;
        desc.label       = label;
        desc.icon        = icon;
        desc.is_selected = is_selected;
        if (is_leaf) {
            desc.flags |= TreeNodeFlag::Leaf;
        }
        return tree_node(desc);
    }

    inline auto tree_node(uint64_t id, CString label, CString icon, bool is_selected, bool is_leaf, ActionRef children) -> TreeItemResponse
    {
        TreeItemDescriptor desc;
        desc.id          = id;
        desc.label       = label;
        desc.icon        = icon;
        desc.is_selected = is_selected;
        if (is_leaf) {
            desc.flags |= TreeNodeFlag::Leaf;
        }
        return tree_node(desc, children);
    }

    // convenience overload for simple nodes / leaves by string id
    inline auto tree_node(CString id_str, CString label, CString icon = nullptr, bool is_selected = false, bool is_leaf = false) -> TreeItemResponse
    {
        TreeItemDescriptor desc;
        desc.id_str      = id_str;
        desc.label       = label;
        desc.icon        = icon;
        desc.is_selected = is_selected;
        if (is_leaf) {
            desc.flags |= TreeNodeFlag::Leaf;
        }
        return tree_node(desc);
    }

    inline auto tree_node(CString id_str, CString label, CString icon, bool is_selected, bool is_leaf, ActionRef children) -> TreeItemResponse
    {
        TreeItemDescriptor desc;
        desc.id_str      = id_str;
        desc.label       = label;
        desc.icon        = icon;
        desc.is_selected = is_selected;
        if (is_leaf) {
            desc.flags |= TreeNodeFlag::Leaf;
        }
        return tree_node(desc, children);
    }

    // convenience helpers for terminal leaves
    inline auto tree_leaf(const TreeItemDescriptor& desc) -> TreeItemResponse
    {
        TreeItemDescriptor copy = desc;
        copy.flags |= TreeNodeFlag::Leaf;
        return tree_node(copy);
    }

    inline auto tree_leaf(uint64_t id, CString label, CString icon = nullptr, bool is_selected = false) -> TreeItemResponse
    {
        return tree_node(id, label, icon, is_selected, true);
    }

    inline auto tree_leaf(CString id_str, CString label, CString icon = nullptr, bool is_selected = false) -> TreeItemResponse
    {
        return tree_node(id_str, label, icon, is_selected, true);
    }

} // namespace lyra::ui

#endif // LYRA_ENGINE_UISYSTEM_WIDGETS_UITREE_H
