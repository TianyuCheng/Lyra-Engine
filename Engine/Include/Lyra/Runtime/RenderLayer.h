#pragma once

#ifndef LYRA_ENGINE_RUNTIME_RENDER_LAYER_H
#define LYRA_ENGINE_RUNTIME_RENDER_LAYER_H

#include <Lyra/Scene/World.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/GeometryCache.h>

// local import
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    /**
     * @brief The RenderLayer manages GPU rendering infrastructure and camera ECS systems.
     *
     * Responsibilities:
     *  - Owns ResourceRetire to manage deferred retirement of GPU resources.
     *  - Exposes convenience template method retire() to defer GPU resource destruction.
     *  - Ticks ResourceRetire at RENDER_PRE (safe deferred GPU resource retirement).
     *  - Flushes all pending retirements at DESTROY after device idle.
     *  - Drives perspective and orthographic camera projection updates at UPDATE_PRE.
     *  - Registers RenderLayer*, ResourceRetire*, and GeometryCache* on the Toolboard.
     *
     * Resources can be retired via:
     *   toolboard.get<ResourceRetire*>()->retire(my_buffer);
     *   toolboard.get<RenderLayer*>()->retire(my_texture, my_view);
     */
    struct RenderLayer
    {
    public:
        /**
         * @brief Default constructor.
         */
        explicit RenderLayer();

        /**
         * @brief Register this layer on the toolboard and bind lifecycle events.
         *        Sizes ResourceRetire from GPUSurface::get_image_count().
         */
        void bind(Application& app);

        /**
         * @brief Called at UPDATE_PRE: update camera projection matrices for all cameras.
         */
        void update(AppContext&);

        /**
         * @brief Called at RENDER_PRE: advance ResourceRetire by one frame.
         */
        void pre_render(AppContext&);

        /**
         * @brief Called at DESTROY: device idle, then drain all pending retirements.
         */
        void destroy(AppContext&);

        /**
         * @brief Immediately drain all pending retirements.
         */
        void drain();

        /**
         * @brief Access the GPU geometry cache.
         */
        GeometryCache&       get_geometry_cache() { return geometry_cache; }
        const GeometryCache& get_geometry_cache() const { return geometry_cache; }

        /**
         * @brief Access the composite ResourceRetire manager.
         */
        ResourceRetire&       get_resource_retire() { return resource_retire; }
        const ResourceRetire& get_resource_retire() const { return resource_retire; }

        /**
         * @brief Retire a GPU resource when in-flight GPU frames complete.
         */
        template <typename... Args>
        void retire(Args&&... args)
        {
            resource_retire.retire(std::forward<Args>(args)...);
        }

        /**
         * @brief Access the underlying typed retirement queue for a given GPU object type.
         */
        template <typename T>
        auto& get_queue()
        {
            return resource_retire.get_queue<T>();
        }

    private:
        // -- GPU resource deferred retirement manager --
        ResourceRetire resource_retire;

        // -- GPU geometry residency cache --
        GeometryCache geometry_cache;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_RENDER_LAYER_H
