#pragma once

#ifndef LYRA_ENGINE_RUNTIME_RENDER_LAYER_H
#define LYRA_ENGINE_RUNTIME_RENDER_LAYER_H

#include <Lyra/Scene/World.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/ResourceRetire.h>
#include <Lyra/Rendering/GeometryCache.h>
#include <Lyra/Rendering/RendererManager.h>
#include <Lyra/Rendering/SystemTextures.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RenderView.h>

// local import
#include <Lyra/Runtime/Application.h>

namespace lyra
{
    struct Camera;
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
        void update(AppContext& context);

        /**
         * @brief Updates camera projection matrices for all Camera components in a World.
         */
        void update_camera_projection(World& world);

        /**
         * @brief Updates projection matrix for a single Camera component.
         */
        static void update_camera_projection(Camera& camera);

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
         * @brief Access the swappable renderer manager.
         */
        RendererManager&       get_renderer_manager() { return renderer_manager; }
        const RendererManager& get_renderer_manager() const { return renderer_manager; }

        /**
         * @brief Access standard built-in fallback system textures and procedural LUTs.
         */
        SystemTextures&       get_system_textures() { return system_textures; }
        const SystemTextures& get_system_textures() const { return system_textures; }

        /**
         * @brief Access extracted scene and view packets for the current frame.
         */
        RenderScene&       get_extracted_scene() { return extracted_scene; }
        const RenderScene& get_extracted_scene() const { return extracted_scene; }
        RenderView&        get_extracted_view() { return extracted_view; }
        const RenderView&  get_extracted_view() const { return extracted_view; }

        /**
         * @brief Extract active camera, lights, and primitives from World into RenderScene and RenderView.
         */
        void extract_scene_and_view(AppContext& context);

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

        // -- pluggable renderer manager --
        RendererManager renderer_manager;

        // -- system fallback textures & procedural LUTs --
        SystemTextures system_textures;

        // -- extracted per-frame scene & view packet --
        RenderScene extracted_scene;
        RenderView  extracted_view;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RUNTIME_RENDER_LAYER_H
