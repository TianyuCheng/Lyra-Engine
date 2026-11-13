#include <imgui.h>
#include <Lyra/UISystem/Widgets/UITree.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    bool draw_tree_item_internal(const void* id_ptr, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef* on_header, ActionRef* content)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (is_selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!content) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));

        bool is_open = ImGui::TreeNodeEx(id_ptr, flags, "%s %s", icon, label);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            on_select();
        }

        if (on_header) {
            (*on_header)();
        }

        if (is_open) {
            if (content) {
                (*content)();
            }
            ImGui::TreePop();
        }

        ImGui::PopStyleVar(2);
        return is_open;
    }
} // namespace

bool lyra::ui::tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, nullptr, nullptr);
}

bool lyra::ui::tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, nullptr, &content);
}

bool lyra::ui::tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef on_header, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, &on_header, &content);
}

bool lyra::ui::tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, nullptr, nullptr);
}

bool lyra::ui::tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, nullptr, &content);
}

bool lyra::ui::tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef on_header, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, &on_header, &content);
}

void lyra::ui::tree_leaf(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, nullptr, nullptr);
}

void lyra::ui::tree_leaf(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, nullptr, nullptr);
}

void lyra::ui::tree_set_next_open(bool is_open)
{
    ImGui::SetNextItemOpen(is_open, ImGuiCond_Appearing);
}

float lyra::ui::calc_tree_item_width(CString label, CString icon, bool reserve_scrollbar)
{
    float width = 0.0f;

    // tree node arrow / indent spacing (matching IndentSpacing = 16.0f)
    width += 16.0f;

    // icon glyph and following space
    if (icon && *icon != '\0') {
        width += ImGui::CalcTextSize(icon).x + ImGui::CalcTextSize(" ").x;
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

