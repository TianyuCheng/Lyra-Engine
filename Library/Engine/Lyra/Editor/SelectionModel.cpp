#include <Lyra/Common/Collections.h>

#include "SelectionModel.h"

using namespace lyra;

bool SelectionModel::is_selected(StringView item) const
{
    return std::find(items.begin(), items.end(), item) != items.end();
}

void SelectionModel::toggle(StringView item)
{
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end()) {
        items.erase(it);
    } else {
        items.emplace_back(item);
    }
    last_selected = item;
}

void SelectionModel::select_only(StringView item)
{
    items.clear();
    items.emplace_back(item);
    last_selected = item;
}

void SelectionModel::select_range(StringView item, const Vector<String>& all_items)
{
    if (last_selected.empty()) {
        select_only(item);
        return;
    }

    int start_idx = -1;
    int end_idx   = -1;

    for (int i = 0; i < (int)all_items.size(); ++i) {
        if (all_items[i] == last_selected) start_idx = i;
        if (all_items[i] == item) end_idx = i;
    }

    if (start_idx != -1 && end_idx != -1) {
        int from = std::min(start_idx, end_idx);
        int to   = std::max(start_idx, end_idx);
        for (int i = from; i <= to; ++i) {
            if (!is_selected(all_items[i])) {
                items.emplace_back(all_items[i]);
            }
        }
    }
}

void SelectionModel::clear()
{
    items.clear();
    last_selected = "";
}
