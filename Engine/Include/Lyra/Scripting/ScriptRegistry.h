#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_SCRIPT_REGISTRY_H
#define LYRA_ENGINE_SCRIPTING_SCRIPT_REGISTRY_H

#include <Lyra/Utilities/Collections.h>
#include <Lyra/Scripting/ScriptTypes.h>

namespace lyra
{
    /**
     * @brief central registry holding statically registered ScriptAPI factory functions.
     * script modules automatically register with this registry at startup via lyra-bindgen.
     */
    struct ScriptRegistry
    {
        using Factory = ScriptAPI (*)();

        /**
         * @brief register a ScriptAPI factory function.
         */
        static void add(Factory factory);

        /**
         * @brief register a static ScriptAPI instance directly.
         */
        static void add(const ScriptAPI& api);

        /**
         * @brief retrieve all registered ScriptAPI factory functions.
         */
        static auto get_factories() -> const Vector<Factory>&;

        /**
         * @brief retrieve all registered ScriptAPI static instances.
         */
        static auto get_apis() -> const Vector<ScriptAPI>&;
    };

    namespace detail
    {
        /**
         * @brief RAII helper invoked by generated static initializers to register a ScriptAPI factory.
         */
        struct ScriptAutoRegistrar
        {
            explicit ScriptAutoRegistrar(ScriptRegistry::Factory factory)
            {
                ScriptRegistry::add(factory);
            }

            explicit ScriptAutoRegistrar(const ScriptAPI& api)
            {
                ScriptRegistry::add(api);
            }
        };
    } // namespace detail

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_SCRIPT_REGISTRY_H
