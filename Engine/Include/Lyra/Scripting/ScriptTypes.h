#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_SCRIPT_TYPES_H
#define LYRA_ENGINE_SCRIPTING_SCRIPT_TYPES_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/String.h>
#include <Lyra/Utilities/GUID.h>
#include <Lyra/Utilities/BitFlags.h>
#include <Lyra/Runtime/AppEnums.h>

namespace lyra
{
    using ScriptID    = GUID;
    using ComponentID = GUID;

    /**
     * @brief System execution flags.
     */
    enum struct ScriptFlag : uint
    {
        NONE          = 0x0,
        RUN_IN_EDITOR = 0x1, ///< Allow system to run while in edit mode
    };
} // namespace lyra

ENABLE_BIT_FLAGS(lyra::ScriptFlag);

namespace lyra
{
    using ScriptFlags = BitFlags<ScriptFlag>;

    /**
     * @brief Query component dependencies declared by a system.
     */
    struct QueryDesc
    {
        const ComponentID* required       = nullptr; ///< components the query iterates
        const bool*        writes         = nullptr; ///< parallel to required; false for const T
        uint               required_count = 0;
        const ComponentID* excluded       = nullptr; ///< Without<T> terms
        uint               excluded_count = 0;
    };

    /**
     * @brief Field descriptor for POD system parameters and reflected properties.
     */
    struct ScriptField
    {
        CString name     = nullptr;
        uint    offset   = 0;
        uint    size     = 0;
        uint    type_tag = 0;
    };

    /**
     * @brief Static metadata descriptor for an ECS script/system.
     */
    struct ScriptDesc
    {
        CString          name        = nullptr;          ///< ScriptID = hash(name), never table index
        CString          group       = nullptr;          ///< "Gameplay/Camera"; Systems panel tree path
        AppEvent         stage       = AppEvent::UPDATE; ///< Pipeline stage when system runs
        ScriptFlags      flags       = ScriptFlag::NONE; ///< e.g. RUN_IN_EDITOR
        uint             state_size  = 0;                ///< engine allocates; script never owns storage
        uint             state_align = 0;
        const QueryDesc* queries     = nullptr; ///< declared by the signature; generated
        uint             query_count = 0;
    };

    struct ScriptContext;

    /**
     * @brief Scripting API exported by a native or dynamic module.
     */
    struct ScriptAPI
    {
        CString (*get_api_name)()                         = nullptr;
        uint (*get_scripts)(ScriptDesc* out)              = nullptr; ///< two-call pattern, house style
        void (*run)(ScriptID id, ScriptContext& ctx)      = nullptr;
        uint (*get_params)(ScriptID id, ScriptField* out) = nullptr; ///< POD field descriptors: per-system settings
    };

    /**
     * @brief Computes a stable, deterministic ScriptID from a script name at compile-time or runtime.
     */
    constexpr auto hash_script_name(StringView name) -> ScriptID
    {
        uint64_t hash = 14695981039346656037ull;
        for (char c : name) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 1099511628211ull;
        }
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ull;
        hash ^= hash >> 27;
        hash *= 0x94d049bb133111ebull;
        hash ^= hash >> 31;
        return (hash == 0) ? 1ull : hash;
    }

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_SCRIPT_TYPES_H
