#pragma once

#ifndef LYRA_ENGINE_RENDERING_RENDER_WORLD_H
#define LYRA_ENGINE_RENDERING_RENDER_WORLD_H

#include <Lyra/Scene/World.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RenderView.h>
#include <Lyra/Rendering/RendererTypes.h>
#include <Lyra/Rendering/GeometryCache.h>
#include <Lyra/Assets/AMSServer.h>

namespace lyra
{
    /**
     * @brief GPU-oriented scene representation and synchronization bridge from ECS World.
     *
     * Extracts active ECS scene components (Camera, Light, Mesh, SDFVolume), ensures
     * GPU buffer residency for requested presentations (Trimesh, Meshlet, BVH),
     * and prepares cullable RenderScene packets for renderers.
     */
    class RenderWorld
    {
    public:
        RenderWorld(GeometryCache& geometry_cache, AssetServer* ams = nullptr);
        ~RenderWorld() = default;

        /**
         * @brief Synchronize and extract the ECS world into the GPU render scene
         *        according to the active renderer's supported primitive features.
         *        todo: evolve to persistent gpu scene with spatial culling and dirty transform tracking.
         */
        void extract(
            const World&                ecs_world,
            const RendererCapabilities& capabilities,
            float                       delta_time = 0.0f);

        [[nodiscard]] const RenderScene& get_scene() const { return scene; }
        [[nodiscard]] RenderScene&       get_scene() { return scene; }
        [[nodiscard]] const RenderView&  get_view() const { return view; }
        [[nodiscard]] RenderView&        get_view() { return view; }

        void set_asset_server(AssetServer* in_ams) { ams = in_ams; }

    private:
        void extract_view(const World& ecs_world);
        void extract_lights(const World& ecs_world);
        void extract_meshes(const World& ecs_world, const RendererCapabilities& capabilities);
        void extract_sdfs(const World& ecs_world);
        void build_acceleration_structures(const RendererCapabilities& capabilities);

    private:
        GeometryCache& geometry_cache;
        AssetServer*   ams = nullptr;
        RenderScene    scene;
        RenderView     view;
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_RENDER_WORLD_H
