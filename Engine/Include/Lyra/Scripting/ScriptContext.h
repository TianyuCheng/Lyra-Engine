#pragma once

#ifndef LYRA_ENGINE_SCRIPTING_SCRIPT_CONTEXT_H
#define LYRA_ENGINE_SCRIPTING_SCRIPT_CONTEXT_H

#include <tuple>
#include <type_traits>
#include <Lyra/Utilities/Macros.h>
#include <Lyra/Utilities/Memory.h>
#include <Lyra/Scene/SceneNode.h>
#include <Lyra/Scene/Transform.h>
#include <Lyra/Scene/World.h>
#include <Lyra/Scripting/Disabled.h>
#include <Lyra/Scripting/ScriptCommandQueue.h>

namespace lyra
{
    /**
     * @brief Range query wrapper enabling structured binding iteration over matching entities.
     * Example: for (auto [node, particle, vel] : ctx.query<Particle, Velocity>()) { ... }
     */
    template <typename... Cs>
    struct Query
    {
        struct Iterator
        {
            using ViewType     = decltype(std::declval<Registry>().view<Cs...>());
            using UnderlyingIt = typename ViewType::iterator;

            Registry*    registry;
            UnderlyingIt it;

            bool operator==(const Iterator& other) const { return it == other.it; }
            bool operator!=(const Iterator& other) const { return it != other.it; }
            auto operator++() -> Iterator&
            {
                ++it;
                return *this;
            }

            auto operator*() const
            {
                Entity ent = *it;
                return std::tuple<SceneNode, Cs&...>(SceneNode(ent), registry->get<Cs>(ent)...);
            }
        };

        explicit Query(World* world = nullptr) : world(world) {}

        auto begin() const
        {
            if (!world) {
                return Iterator{nullptr, {}};
            }
            auto v = world->registry.view<Cs...>();
            return Iterator{&world->registry, v.begin()};
        }

        auto end() const
        {
            if (!world) {
                return Iterator{nullptr, {}};
            }
            auto v = world->registry.view<Cs...>();
            return Iterator{&world->registry, v.end()};
        }

    private:
        World* world = nullptr;
    };

    /**
     * @brief Facade execution context passed to ECS script systems.
     * Provides timing, scratch memory, transform manipulation, queries, and deferred commands.
     * Does NOT expose raw World& or Registry& to scripts.
     */
    struct ScriptContext
    {
    public:
        explicit ScriptContext(World* world, ScriptCommandQueue* queue, MemoryArena* scratch, float dt, float time);

        // Frame timing
        FORCE_INLINE float dt() const { return delta_time; }
        FORCE_INLINE float time() const { return total_time; }

        // Per-frame temporary scratch memory
        FORCE_INLINE auto scratch() -> MemoryArena& { return *scratch_arena; }

        // Deferred entity commands
        FORCE_INLINE void destroy(SceneNode node)
        {
            if (!cmd_queue) {
                return;
            }
            cmd_queue->destroy(node);
        }

        FORCE_INLINE void create(Function<void(SceneNode)> on_created = {})
        {
            if (!cmd_queue) {
                return;
            }
            cmd_queue->create(std::move(on_created));
        }

        template <typename T, typename... Args>
        FORCE_INLINE void add_component(SceneNode node, Args&&... args)
        {
            if (!cmd_queue) {
                return;
            }
            cmd_queue->add_component<T>(node, std::forward<Args>(args)...);
        }

        template <typename T>
        FORCE_INLINE void remove_component(SceneNode node)
        {
            if (!cmd_queue) {
                return;
            }
            cmd_queue->remove_component<T>(node);
        }

        // Component access
        template <typename T>
        FORCE_INLINE auto get(SceneNode node) -> T&
        {
            return world->registry.get<T>(node.entity);
        }

        template <typename T>
        FORCE_INLINE auto get(SceneNode node) const -> const T&
        {
            return world->registry.get<T>(node.entity);
        }

        template <typename T>
        FORCE_INLINE auto try_get(SceneNode node) -> T*
        {
            return world->registry.try_get<T>(node.entity);
        }

        template <typename T>
        FORCE_INLINE auto try_get(SceneNode node) const -> const T*
        {
            return world->registry.try_get<T>(node.entity);
        }

        template <typename... T>
        FORCE_INLINE bool has(SceneNode node) const
        {
            return world->registry.all_of<T...>(node.entity);
        }

        // Transform helpers
        void rotate(TransformLocal& transform, const Vector3& axis, float angle_deg);
        void rotate(TransformLocal& transform, const Quaternion& rot);
        void translate(TransformLocal& transform, const Vector3& translation);
        void scale(TransformLocal& transform, const Vector3& scale);

        void rotate(SceneNode node, const Vector3& axis, float angle_deg);
        void rotate(SceneNode node, const Quaternion& rot);
        void translate(SceneNode node, const Vector3& translation);
        void scale(SceneNode node, const Vector3& scale);

        // General query factory
        template <typename... Cs>
        FORCE_INLINE auto query() -> Query<Cs...>
        {
            return Query<Cs...>(world);
        }

        // Batched per-entity iteration
        template <typename... Cs, typename Callable>
        void each(Callable&& callable)
        {
            if (!world) {
                return;
            }

            // check if any Disabled<C> component is currently active in the registry
            bool has_disabled   = false;
            auto check_disabled = [&]<typename C>() {
                const auto* storage = std::as_const(world->registry).template storage<Disabled<std::decay_t<C>>>();
                if (storage && !storage->empty()) {
                    has_disabled = true;
                }
            };
            (check_disabled.template operator()<Cs>(), ...);

            auto invoke_callable = [&](auto entity, auto&&... components) {
                if constexpr (std::is_invocable_v<Callable, SceneNode, decltype(components)...>) {
                    callable(SceneNode(entity), components...);
                } else if constexpr (std::is_invocable_v<Callable, decltype(components)...>) {
                    callable(components...);
                }
            };

            if (has_disabled) {
                auto v = world->registry.template view<Cs...>(entt::exclude<Disabled<std::decay_t<Cs>>...>);
                v.each(invoke_callable);
                return;
            }

            auto v = world->registry.template view<Cs...>();
            v.each(invoke_callable);
        }

    private:
        World*              world         = nullptr;
        ScriptCommandQueue* cmd_queue     = nullptr;
        MemoryArena*        scratch_arena = nullptr;
        float               delta_time    = 0.0f;
        float               total_time    = 0.0f;
    };

} // namespace lyra

#endif // LYRA_ENGINE_SCRIPTING_SCRIPT_CONTEXT_H
