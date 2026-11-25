#pragma once

#ifndef LYRA_ENGINE_RENDERING_SCENE_LIGHTING_H
#define LYRA_ENGINE_RENDERING_SCENE_LIGHTING_H

#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/FrameArena.h>
#include <Lyra/Rendering/ShadingBuffers.h>

namespace lyra
{
    /**
     * @brief Manages scene lighting data and transient GPU staging for a frame.
     */
    struct SceneLighting
    {
        GPUBufferSlice lights_slice{};
        uint           directional_light_count = 0;
        uint           local_light_count       = 0;
        uint           total_light_count       = 0;

        /// Stages scene lights into the frame arena as a structured buffer slice.
        void init(const RenderScene& scene, FrameArena& arena, GPUDevice device = {})
        {
            directional_light_count = static_cast<uint>(scene.directional_lights.size());
            local_light_count       = static_cast<uint>(scene.local_lights.size());
            total_light_count       = directional_light_count + local_light_count;

            Vector<GPULightData> lights;
            lights.reserve(std::max<usize>(1, total_light_count));

            for (const auto& dl : scene.directional_lights) {
                GPULightData l{};
                l.position     = dl.position;
                l.range        = dl.range;
                l.direction    = dl.direction;
                l.intensity    = dl.intensity;
                l.color        = dl.color;
                l.type         = static_cast<uint>(RenderLightType::DIRECTIONAL);
                l.cast_shadows = dl.cast_shadows ? 1 : 0;
                lights.push_back(l);
            }

            for (const auto& ll : scene.local_lights) {
                GPULightData l{};
                l.position         = ll.position;
                l.range            = ll.range;
                l.direction        = ll.direction;
                l.intensity        = ll.intensity;
                l.color            = ll.color;
                l.type             = static_cast<uint>(ll.type);
                l.spot_inner_angle = ll.spot_inner_angle;
                l.spot_outer_angle = ll.spot_outer_angle;
                l.cast_shadows     = ll.cast_shadows ? 1 : 0;
                lights.push_back(l);
            }

            if (lights.empty()) {
                lights.push_back(GPULightData{});
            }

            lights_slice = arena.create_uniform(lights.data(), lights.size() * sizeof(GPULightData), 256, device);
        }

        void reset()
        {
            lights_slice            = {};
            directional_light_count = 0;
            local_light_count       = 0;
            total_light_count       = 0;
        }

        [[nodiscard]] auto valid() const -> bool
        {
            return lights_slice.valid();
        }
    };
} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_SCENE_LIGHTING_H
