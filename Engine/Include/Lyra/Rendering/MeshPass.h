#pragma once

#ifndef LYRA_ENGINE_RENDERING_MESH_PASS_H
#define LYRA_ENGINE_RENDERING_MESH_PASS_H

#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Stdint.h>
#include <Lyra/Utilities/Collections.h>
#include <Lyra/Utilities/Function.h>
#include <Lyra/Graphics/RHITypes.h>
#include <Lyra/Rendering/RenderScene.h>
#include <Lyra/Rendering/RenderView.h>

namespace lyra
{

    /**
     * @brief 64-bit sort key for state sorting: minimizes pipeline and buffer changes, sorts depth.
     */
    struct RenderSortKey
    {
        ulong value = 0;

        static auto make(uint pipeline_id, uint buffer_id, float depth, bool translucent = false) -> RenderSortKey
        {
            ushort depth_quant = static_cast<ushort>(glm::clamp(depth, 0.0f, 1.0f) * 65535.0f);
            if (translucent) {
                depth_quant = 65535 - depth_quant;
            }

            ulong v = (static_cast<ulong>(pipeline_id) << 32) | (static_cast<ulong>(buffer_id) << 16) | static_cast<ulong>(depth_quant);
            return {v};
        }

        auto operator<=>(const RenderSortKey&) const = default;
    };

    /**
     * @brief Per-instance immediate push constants for vertex pulling and instance data.
     * @note Memory layout directly mirrors the GPU shader layout of struct `InstanceData` in
     *       `sys://Shaders/Forward/Forward.slang`. Any layout modification must be kept strictly
     *       in sync across both C++ and Slang shader definitions.
     */
    struct alignas(16) MeshDrawInstance
    {
        Matrix4x4 model;
        Vector4   base_color      = Vector4(1.0f);
        float     roughness       = 0.5f;
        float     metallic        = 0.0f;
        float     clear_coat      = 0.0f;
        uint      material_id     = 0;
        uint      position_offset = 0;
        uint      normal_offset   = 0;
        uint      uv_offset       = 0;
        uint      padding1        = 0;
    };

    /**
     * @brief atomic mesh draw command.
     */
    struct MeshDrawCommand
    {
        MeshDrawInstance instance;
        GPUBufferHandle  index_buffer;
        GPUIndexFormat   index_format = GPUIndexFormat::UINT32;
        GPUSize64        index_offset = 0;
        uint             index_count  = 0;
        uint             first_index  = 0;
    };

    /**
     * @brief contiguous batch of draw commands sharing the same pipeline and vertex buffer.
     */
    struct MeshDrawBatch
    {
        GPURenderPipeline       pipeline;
        GPUBufferHandle         vertex_buffer;
        GPUBindGroup            vertex_bind_group;
        Vector<MeshDrawCommand> commands;
    };

    /**
     * @brief pass execution coordinator for filtering, sorting, and dispatching mesh draw batches.
     */
    class MeshPass
    {
    public:
        using PipelineResolver = FunctionRef<GPURenderPipeline(const MeshPrimitive&)>;

        MeshPass() = default;
        ~MeshPass();

        MeshPass(const MeshPass&)            = delete;
        MeshPass& operator=(const MeshPass&) = delete;

        MeshPass(MeshPass&&) noexcept            = default;
        MeshPass& operator=(MeshPass&&) noexcept = default;

        bool init(GPUDevice dev, GPUBindGroupLayoutHandle in_vertex_layout);
        void destroy();

        void prepare(
            GPUDevice          device,
            const RenderScene& scene,
            const RenderView&  view,
            PipelineResolver   resolve_pipeline);

        void draw(GPUCommandEncoder& command, GPUBindGroup pass_bind_group = {}) const;
        void clear();

        [[nodiscard]] uint get_draw_call_count() const { return draw_calls; }
        [[nodiscard]] uint get_triangle_count() const { return triangles_rendered; }

    private:
        GPUDevice                    device;
        GPUBindGroupLayoutHandle     vertex_layout;
        GPUBindGroupHeap             heap;
        HashMap<ulong, GPUBindGroup> geometry_bind_groups;
        Vector<MeshDrawBatch>        batches;
        uint                         draw_calls         = 0;
        uint                         triangles_rendered = 0;
        bool                         initialized        = false;
    };

} // namespace lyra

#endif // LYRA_ENGINE_RENDERING_MESH_PASS_H
