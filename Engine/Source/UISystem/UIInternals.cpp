#include <Lyra/UISystem/UIInternals.h>

using namespace lyra;
using namespace lyra::ui;
using namespace lyra::ui::internal;

namespace lyra::ui::internal
{
    thread_local std::stack<LayoutScope> g_layout_stack;
    thread_local int                     g_row_counter = 0;
} // namespace lyra::ui::internal

using namespace lyra;
using namespace lyra::ui;

void internal::advance_layout_item()
{
    if (g_layout_stack.empty()) return;

    auto& current = g_layout_stack.top();
    if (current.in_row) {
        if (current.has_spacer_pending) {
            current.has_spacer_pending = false;
        } else if (!current.is_first_item) {
            ImGui::SameLine();
        }
        current.is_first_item = false;
    }
}

bool internal::is_vertically_centered()
{
    if (g_layout_stack.empty()) return false;
    const auto& current = g_layout_stack.top();
    return current.in_row && (current.vertical == VAlign::Center || current.vertical == VAlign::Baseline);
}

void internal::reset_layout_counters()
{
    g_row_counter = 0;
}
