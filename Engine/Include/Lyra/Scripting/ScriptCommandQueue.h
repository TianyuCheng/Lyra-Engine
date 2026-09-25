#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_SCRIPT_COMMAND_QUEUE_H
#define LYRA_ENGINE_SCRIPTING_SCRIPT_COMMAND_QUEUE_H

#include <tuple>
#include <utility>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/World.h>

namespace lyra
{
    /**
     * @brief Queue for deferred structural changes executed during system iteration
     * to prevent iterator invalidation.
     */
    struct ScriptCommandQueue
    {
    public:
        ScriptCommandQueue() = default;

        /**
         * @brief Enqueue deferred creation of a SceneNode.
         * @param on_created Optional callback invoked with the created node upon flush.
         */
        void create(Function<void(SceneNode)> on_created = {});

        /**
         * @brief Enqueue deferred destruction of a SceneNode.
         */
        void destroy(SceneNode node);

        /**
         * @brief Enqueue deferred addition/construction of a component on a SceneNode.
         */
        template <typename T, typename... Args>
        void add_component(SceneNode node, Args&&... args)
        {
            auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
            commands.emplace_back([node, tuple = std::move(args_tuple)](World& world) mutable {
                std::apply([&](auto&&... unpacked) {
                    world.registry.emplace_or_replace<T>(node.entity, std::forward<decltype(unpacked)>(unpacked)...);
                }, std::move(tuple));
            });
        }

        /**
         * @brief Enqueue deferred removal of a component from a SceneNode.
         */
        template <typename T>
        void remove_component(SceneNode node)
        {
            commands.emplace_back([node](World& world) {
                world.registry.remove<T>(node.entity);
            });
        }

        /**
         * @brief Flushes all queued commands into the target World and clears the queue.
         */
        void flush(World& world);

        /**
         * @brief Clears all queued commands without executing them.
         */
        void clear();

        /**
         * @brief Returns the count of pending commands.
         */
        size_t size() const;

        /**
         * @brief Returns true if there are no pending commands.
         */
        bool empty() const;

    private:
        Vector<Function<void(World&)>> commands;
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_SCRIPT_COMMAND_QUEUE_H
