#include <Lyra/Scripting/ScriptQuery.h>
#include <Lyra/Runtime/ScriptLayer.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/SceneNode.h>

using namespace lyra;

bool lyra::matches_system_query(
    const QueryDescriptor&      query,
    const HashSet<ComponentID>& entity_components)
{
    if (query.required_count == 0 || !query.required) {
        return false;
    }

    for (uint i = 0; i < query.required_count; ++i) {
        if (!entity_components.contains(query.required[i])) {
            return false;
        }
    }

    if (query.excluded_count > 0 && query.excluded) {
        for (uint i = 0; i < query.excluded_count; ++i) {
            if (entity_components.contains(query.excluded[i])) {
                return false;
            }
        }
    }

    return true;
}

String lyra::format_query_access(
    const QueryDescriptor&              query,
    const HashMap<ComponentID, String>& component_names)
{
    String summary;
    for (uint i = 0; i < query.required_count; ++i) {
        if (!summary.empty()) {
            summary += ", ";
        }
        ComponentID cid = query.required[i];
        auto        it  = component_names.find(cid);
        if (it != component_names.end()) {
            summary += it->second;
        } else {
            summary += "Component";
        }
        summary += (query.writes && query.writes[i]) ? " (RW)" : " (R)";
    }
    return summary;
}

void lyra::collect_entity_components(
    const ScriptLayer&            scripting,
    World&                        world,
    SceneNode                     node,
    HashSet<ComponentID>&         out_components,
    HashMap<ComponentID, String>& out_names)
{
    for (const auto& comp : scripting.get_components()) {
        ComponentID cid = hash_script_name(comp.name);
        out_names[cid]  = comp.name;
        if (comp.has_component && comp.has_component(world, node)) {
            out_components.insert(cid);
        }
    }
}

RelatedSystems lyra::find_related_systems(
    const ScriptLayer& scripting,
    World&             world,
    SceneNode          node)
{
    HashSet<ComponentID>         entity_components;
    HashMap<ComponentID, String> component_names;
    collect_entity_components(scripting, world, node, entity_components, component_names);

    RelatedSystems results;
    for (const auto& sys : scripting.get_scripts()) {
        if (sys.query_count == 0 || !sys.queries) {
            continue;
        }

        bool   matches = false;
        String summary;

        SmallVector<ComponentAccess, 4> accessed_components;
        for (uint qi = 0; qi < sys.query_count; ++qi) {
            const auto& q = sys.queries[qi];
            if (matches_system_query(q, entity_components)) {
                matches      = true;
                String q_str = format_query_access(q, component_names);
                if (!q_str.empty()) {
                    if (!summary.empty()) {
                        summary += " | ";
                    }
                    summary += q_str;
                }

                for (uint i = 0; i < q.required_count; ++i) {
                    ComponentID     cid = q.required[i];
                    ComponentAccess access;
                    access.id    = cid;
                    access.write = (q.writes && q.writes[i]);

                    auto it = component_names.find(cid);
                    if (it != component_names.end()) {
                        access.name = it->second;
                    }

                    accessed_components.push_back(access);
                }
            }
        }

        if (matches) {
            results.push_back({
                .descriptor          = &sys,
                .accessed_components = std::move(accessed_components),
                .access_summary      = std::move(summary),
            });
        }
    }

    return results;
}
