#pragma once

#ifndef LYRA_ENGINE_RUNTIME_SCRIPT_LAYER_H
#define LYRA_ENGINE_RUNTIME_SCRIPT_LAYER_H

#include <Lyra/Utilities/Memory.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Runtime/AppEnums.h>
#include <Lyra/Runtime/Application.h>
#include <Lyra/Scripting/ScriptTypes.h>
#include <Lyra/Scripting/ScriptCommandQueue.h>
#include <Lyra/Scripting/ScriptContext.h>

namespace lyra
{
    /**
     * @brief Runtime layer managing script registration, state storage, stage dispatching,
     * and simulation lifecycle.
     */
    struct ScriptLayer
    {
    public:
        explicit ScriptLayer();
        ~ScriptLayer() = default;

        /**
         * @brief Registers the layer with the Application toolboard and binds stage events.
         */
        void bind(Application& app);

        /**
         * @brief Dispatches UPDATE_PRE stage systems.
         */
        void run_pre(AppContext& context);

        /**
         * @brief Dispatches UPDATE stage systems.
         */
        void run_main(AppContext& context);

        /**
         * @brief Dispatches UPDATE_POST stage systems and flushes scratch memory.
         */
        void run_post(AppContext& context);

        /**
         * @brief Registers a ScriptAPI table exported by a native or module library.
         */
        void register_api(const ScriptAPI& api);

        /**
         * @brief Unregisters all registered APIs, scripts, and state.
         */
        void unregister_all();

        /**
         * @brief Controls the simulation state (Edit vs Play vs Paused).
         */
        void            set_simulation_state(SimulationState state);
        SimulationState get_simulation_state() const;

        /**
         * @brief Sets whether a specific script/system is enabled.
         */
        void set_script_enabled(ScriptID id, bool enabled);
        bool is_script_enabled(ScriptID id) const;

        /**
         * @brief Sets whether all systems in a specific group path are enabled.
         */
        void set_group_enabled(StringView group, bool enabled);
        bool is_group_enabled(StringView group) const;

        /**
         * @brief Gets all registered script descriptors.
         */
        const Vector<ScriptDesc>& get_scripts() const;

        /**
         * @brief Gets all registered ScriptAPI tables.
         */
        const Vector<ScriptAPI>& get_apis() const;

        /**
         * @brief Accesses the layer's deferred command queue.
         */
        ScriptCommandQueue& get_command_queue();

        /**
         * @brief Retrieves engine-allocated raw POD state for a system.
         */
        uint8_t* get_script_state(ScriptID id);

    private:
        void dispatch_stage(AppEvent stage, AppContext& context);

        SimulationState                    simulation_state = SimulationState::EDIT;
        ScriptCommandQueue                 command_queue;
        Vector<ScriptAPI>                  apis;
        Vector<ScriptDesc>                 scripts;
        HashMap<ScriptID, bool>            script_enabled;
        HashMap<String, bool>              group_enabled;
        HashMap<ScriptID, Vector<uint8_t>> script_states;
        detail::MemoryArena                scratch_arena;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_SCRIPT_LAYER_H
