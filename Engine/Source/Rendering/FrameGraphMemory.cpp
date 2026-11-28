#include <algorithm>
#include <Lyra/Rendering/FrameGraphMemory.h>

using namespace lyra;

MemoryAliasingPlan FrameGraphMemoryPlanner::compute_plan(View<ResourceLifetimeInterval> intervals)
{
    MemoryAliasingPlan plan;
    plan.resource_hints.resize(intervals.size(), GPUMemoryHint::dedicated());
    plan.has_aliased_predecessor.resize(intervals.size(), false);
    plan.predecessor_physical_id.resize(intervals.size(), ~0u);

    // gather indices of active transient resources
    Vector<uint> active_indices;
    active_indices.reserve(intervals.size());
    for (uint i = 0; i < static_cast<uint>(intervals.size()); ++i) {
        if (intervals[i].is_active) {
            active_indices.push_back(i);
        }
    }

    if (active_indices.empty()) {
        return plan;
    }

    // sort intervals by start_pass ascending (tie-break by longer lifetime first)
    std::sort(active_indices.begin(), active_indices.end(), [&](uint a, uint b) {
        if (intervals[a].start_pass != intervals[b].start_pass) {
            return intervals[a].start_pass < intervals[b].start_pass;
        }
        return intervals[a].end_pass > intervals[b].end_pass;
    });

    // min-heap storing {end_pass, color_slot}
    using ActiveSlot = std::pair<uint, uint>;
    PriorityQueue<ActiveSlot, Vector<ActiveSlot>, std::greater<>> active_slots;

    Vector<uint>        free_slots;
    HashMap<uint, uint> slot_last_owner;
    uint                next_slot = 1;

    for (uint idx : active_indices) {
        const auto& item = intervals[idx];

        // reclaim slots from resources whose lifetimes expired before item.start_pass
        while (!active_slots.empty() && active_slots.top().first < item.start_pass) {
            free_slots.push_back(active_slots.top().second);
            active_slots.pop();
        }

        uint assigned_slot = 0;
        if (!free_slots.empty()) {
            assigned_slot = free_slots.back();
            free_slots.pop_back();

            plan.has_aliased_predecessor[idx] = true;
            if (auto it = slot_last_owner.find(assigned_slot); it != slot_last_owner.end()) {
                plan.predecessor_physical_id[idx] = it->second;
            }
        } else {
            assigned_slot = next_slot++;
        }

        slot_last_owner[assigned_slot] = item.physical_id != ~0u ? item.physical_id : idx;
        plan.resource_hints[idx]       = GPUMemoryHint::aliased(assigned_slot);
        active_slots.push({item.end_pass, assigned_slot});
    }

    plan.max_color_slots = next_slot - 1;
    return plan;
}
