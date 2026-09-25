#include <Lyra/Scripting/ScriptCommandQueue.h>

using namespace lyra;

void ScriptCommandQueue::create(Function<void(SceneNode)> on_created)
{
    commands.emplace_back([cb = std::move(on_created)](World& world) mutable {
        SceneNode node = world.create();
        if (cb) {
            cb(node);
        }
    });
}

void ScriptCommandQueue::destroy(SceneNode node)
{
    commands.emplace_back([node](World& world) {
        world.registry.destroy(node.entity);
    });
}

void ScriptCommandQueue::flush(World& world)
{
    if (commands.empty()) {
        return;
    }

    // execute each command in queue order
    for (auto& cmd : commands) {
        if (!cmd) {
            continue;
        }
        cmd(world);
    }
    commands.clear();
}

void ScriptCommandQueue::clear()
{
    commands.clear();
}

size_t ScriptCommandQueue::size() const
{
    return commands.size();
}

bool ScriptCommandQueue::empty() const
{
    return commands.empty();
}
