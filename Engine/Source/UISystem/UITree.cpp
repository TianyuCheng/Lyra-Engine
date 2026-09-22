#include <imgui.h>
#include <Lyra/UISystem/UITree.h>

using namespace lyra;
using namespace lyra::ui;

namespace
{
    bool draw_tree_item_internal(const void* id_ptr, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef* content)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (is_selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!content) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool is_open = ImGui::TreeNodeEx(id_ptr, flags, "%s %s", icon, label);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            on_select();
        }

        if (is_open) {
            if (content) {
                (*content)();
            }
            ImGui::TreePop();
        }

        return is_open;
    }
}

bool lyra::ui::tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, nullptr);
}

bool lyra::ui::tree_item(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, &content);
}

bool lyra::ui::tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, nullptr);
}

bool lyra::ui::tree_item(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select, ActionRef content)
{
    return draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, &content);
}

void lyra::ui::tree_leaf(uint64_t id, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    draw_tree_item_internal(reinterpret_cast<const void*>(static_cast<uintptr_t>(id)), icon, label, is_selected, on_select, nullptr);
}

void lyra::ui::tree_leaf(CString id_str, CString icon, CString label, bool is_selected, ActionRef on_select)
{
    draw_tree_item_internal(reinterpret_cast<const void*>(id_str), icon, label, is_selected, on_select, nullptr);
}
