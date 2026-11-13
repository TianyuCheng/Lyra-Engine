#include <imgui.h>
#include <Lyra/UISystem/Widgets/UITree.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    TreeItemResponse draw_tree_node_internal(const TreeItemDescriptor& desc, ActionRef* on_header, ActionRef* children)
    {
        // handle imperative/declarative opening flags
        if (desc.flags.contains(TreeNodeFlag::ForceOpen)) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        } else if (desc.flags.contains(TreeNodeFlag::DefaultOpen)) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
        }

        ImGuiTreeNodeFlags im_flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (desc.flags.contains(TreeNodeFlag::SpanFullWidth)) {
            im_flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
        }
        if (desc.is_selected) {
            im_flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (desc.flags.contains(TreeNodeFlag::Leaf) || !children) {
            im_flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (desc.flags.contains(TreeNodeFlag::Framed)) {
            im_flags |= ImGuiTreeNodeFlags_Framed;
        }
        if (desc.flags.contains(TreeNodeFlag::OpenOnDoubleClick)) {
            im_flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
        }

        // resolve stable imgui id
        const void* id_ptr = nullptr;
        if (desc.id != 0) {
            id_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(desc.id));
        } else if (desc.id_str && *desc.id_str != '\0') {
            id_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(ImGui::GetID(desc.id_str)));
        } else if (desc.label && *desc.label != '\0') {
            id_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(ImGui::GetID(desc.label)));
        } else {
            id_ptr = reinterpret_cast<const void*>(static_cast<uintptr_t>(0));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

        bool is_open = false;
        if (desc.icon && *desc.icon != '\0') {
            is_open = ImGui::TreeNodeEx(id_ptr, im_flags, "%s  %s", desc.icon, desc.label ? desc.label : "");
        } else {
            is_open = ImGui::TreeNodeEx(id_ptr, im_flags, "%s", desc.label ? desc.label : "");
        }

        TreeItemResponse response;
        response.is_open           = is_open;
        response.is_clicked        = ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen();
        response.is_double_clicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen();
        response.is_right_clicked  = ImGui::IsItemClicked(ImGuiMouseButton_Right);
        response.is_toggled_open   = ImGui::IsItemToggledOpen();
        response.is_hovered        = ImGui::IsItemHovered();

        // invoke header callback while the tree node header is the current item
        // (enables immediate attachment of drag & drop source/target, context menu, etc.)
        if (on_header) {
            (*on_header)();
        }

        // render children if open
        if (is_open) {
            if (children) {
                (*children)();
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleVar(2);
        return response;
    }
} // namespace

TreeItemResponse lyra::ui::tree_node(const TreeItemDescriptor& desc)
{
    return draw_tree_node_internal(desc, nullptr, nullptr);
}

TreeItemResponse lyra::ui::tree_node(const TreeItemDescriptor& desc, ActionRef children)
{
    return draw_tree_node_internal(desc, nullptr, &children);
}

TreeItemResponse lyra::ui::tree_node(const TreeItemDescriptor& desc, ActionRef on_header, ActionRef children)
{
    return draw_tree_node_internal(desc, &on_header, &children);
}

void lyra::ui::tree_set_next_open(bool is_open)
{
    ImGui::SetNextItemOpen(is_open, ImGuiCond_Always);
}

float lyra::ui::calc_tree_item_width(CString label, CString icon, bool reserve_scrollbar)
{
    float width = 0.0f;

    // tree node arrow / indent spacing (matching IndentSpacing = 16.0f)
    width += 16.0f;

    // icon glyph and following space
    if (icon && *icon != '\0') {
        width += ImGui::CalcTextSize(icon).x + ImGui::CalcTextSize("  ").x;
    }

    // label text
    if (label && *label != '\0') {
        width += ImGui::CalcTextSize(label).x;
    }

    // frame padding (matching FramePadding.x = 4.0f)
    width += 4.0f * 2.0f;

    // container window padding
    width += ImGui::GetStyle().WindowPadding.x * 2.0f;

    // reserve space for vertical scrollbar and spacing so items never clip
    if (reserve_scrollbar) {
        width += ImGui::GetStyle().ScrollbarSize + ImGui::GetStyle().ItemSpacing.x;
    }

    return width;
}
