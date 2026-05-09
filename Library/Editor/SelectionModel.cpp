#include <Lyra/Common/Collections.h>

#include <Lyra/Editor/SelectionModel.h>

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
}

void SelectionModel::select_only(StringView item)
{
    items.clear();
    items.emplace_back(item);
}

void SelectionModel::clear()
{
    items.clear();
}
