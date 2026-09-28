#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_SCRIPT_QUERY_H
#define LYRA_ENGINE_SCRIPTING_SCRIPT_QUERY_H

#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Scripting/ScriptTypes.h>

namespace lyra
{
    struct World;
    struct SceneNode;
    struct ScriptLayer;

    /**
     * @brief Detailed component access information for an ECS system query.
     */
    struct ComponentAccess
    {
        ComponentID id = 0;
        StringView  name;
        bool        write = false;
    };

    /**
     * @brief Metadata for a system matching an entity's component configuration.
     */
    struct RelatedSystem
    {
        const ScriptDescriptor*         descriptor = nullptr;
        SmallVector<ComponentAccess, 4> accessed_components;
        String                          access_summary;
    };

    using RelatedSystems = SmallVector<RelatedSystem, 8>;

    /**
     * @brief Evaluates whether a QueryDescriptor matches a set of entity components.
     */
    bool matches_system_query(
        const QueryDescriptor&      query,
        const HashSet<ComponentID>& entity_components);

    /**
     * @brief Formats access summary string for a system query.
     */
    String format_query_access(
        const QueryDescriptor&              query,
        const HashMap<ComponentID, String>& component_names);

    /**
     * @brief Collects all component IDs and name mappings for a given entity.
     */
    void collect_entity_components(
        const ScriptLayer&            scripting,
        World&                        world,
        SceneNode                     node,
        HashSet<ComponentID>&         out_components,
        HashMap<ComponentID, String>& out_names);

    /**
     * @brief Finds all systems in the script layer that match the given entity.
     */
    RelatedSystems find_related_systems(
        const ScriptLayer& scripting,
        World&             world,
        SceneNode          node);

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_SCRIPT_QUERY_H
