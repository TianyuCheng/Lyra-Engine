#include <Lyra/Runtime/ScriptLayer.h>
#include <Lyra/Runtime/TimingLayer.h>
#include <Lyra/Scene/World.h>

using namespace lyra;

ScriptLayer::ScriptLayer() : scratch_arena(64 * 1024)
{
}

void ScriptLayer::bind(Application& app)
{
    app.get_toolboard().add<ScriptLayer*>(this);
    app.bind<AppEvent::UPDATE_PRE, &ScriptLayer::run_pre>(*this);
    app.bind<AppEvent::UPDATE, &ScriptLayer::run_main>(*this);
    app.bind<AppEvent::UPDATE_POST, &ScriptLayer::run_post>(*this);
}

void ScriptLayer::run_pre(AppContext& context)
{
    dispatch_stage(AppEvent::UPDATE_PRE, context);
}

void ScriptLayer::run_main(AppContext& context)
{
    dispatch_stage(AppEvent::UPDATE, context);
}

void ScriptLayer::run_post(AppContext& context)
{
    dispatch_stage(AppEvent::UPDATE_POST, context);
    // reset scratch arena at end of frame
    scratch_arena.reset();
}

void ScriptLayer::register_api(const ScriptAPI& api)
{
    apis.push_back(api);

    if (!api.get_scripts) {
        return;
    }

    uint count = api.get_scripts(nullptr);
    if (count == 0) {
        return;
    }

    Vector<ScriptDesc> descs(count);
    api.get_scripts(descs.data());

    for (const auto& desc : descs) {
        scripts.push_back(desc);
        ScriptID id = hash_script_name(desc.name);

        if (script_enabled.find(id) == script_enabled.end()) {
            script_enabled[id] = true;
        }

        if (desc.state_size > 0 && script_states.find(id) == script_states.end()) {
            script_states[id].resize(desc.state_size, 0);
        }
    }
}

void ScriptLayer::unregister_all()
{
    apis.clear();
    scripts.clear();
    script_enabled.clear();
    group_enabled.clear();
    script_states.clear();
    command_queue.clear();
    scratch_arena.reset();
}

void ScriptLayer::set_simulation_state(SimulationState state)
{
    simulation_state = state;
}

SimulationState ScriptLayer::get_simulation_state() const
{
    return simulation_state;
}

void ScriptLayer::set_script_enabled(ScriptID id, bool enabled)
{
    script_enabled[id] = enabled;
}

bool ScriptLayer::is_script_enabled(ScriptID id) const
{
    auto it = script_enabled.find(id);
    if (it == script_enabled.end()) {
        return true;
    }
    return it->second;
}

void ScriptLayer::set_group_enabled(StringView group, bool enabled)
{
    group_enabled[String(group)] = enabled;
}

bool ScriptLayer::is_group_enabled(StringView group) const
{
    auto it = group_enabled.find(String(group));
    if (it == group_enabled.end()) {
        return true;
    }
    return it->second;
}

const Vector<ScriptDesc>& ScriptLayer::get_scripts() const
{
    return scripts;
}

const Vector<ScriptAPI>& ScriptLayer::get_apis() const
{
    return apis;
}

ScriptCommandQueue& ScriptLayer::get_command_queue()
{
    return command_queue;
}

uint8_t* ScriptLayer::get_script_state(ScriptID id)
{
    auto it = script_states.find(id);
    if (it == script_states.end() || it->second.empty()) {
        return nullptr;
    }
    return it->second.data();
}

void ScriptLayer::dispatch_stage(AppEvent stage, AppContext& context)
{
    if (simulation_state == SimulationState::PAUSED) {
        return;
    }

    World* world = context.try_tool<World>();
    if (!world) {
        return;
    }

    Clock* clock = context.try_tool<Clock>();
    float dt   = clock ? clock->delta_time : 0.0f;
    float time = clock ? clock->total_time : 0.0f;

    ScriptContext ctx(world, &command_queue, &scratch_arena, dt, time);

    for (const auto& desc : scripts) {
        if (desc.stage != stage) {
            continue;
        }

        ScriptID id = hash_script_name(desc.name);

        if (!is_script_enabled(id)) {
            continue;
        }

        if (desc.group && !is_group_enabled(desc.group)) {
            continue;
        }

        if (simulation_state == SimulationState::EDIT && !desc.flags.contains(ScriptFlag::RUN_IN_EDITOR)) {
            continue;
        }

        for (const auto& api : apis) {
            if (!api.run) {
                continue;
            }
            api.run(id, ctx);
        }
    }

    // flush deferred structural mutations after stage execution
    command_queue.flush(*world);
}
