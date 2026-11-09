#include <Lyra/Rendering/Passes/LightCullingPass.h>

using namespace lyra;
using namespace lyra::passes;

LightCullingData LightCullingPass::add(
    FrameGraphBuilder& builder,
    const RenderView&  view,
    const RenderScene& scene)
{
    return builder.add_pass<LightCullingData>(
        "LightCullingPass",
        [&](LightCullingData& data, FrameGraphPassBuilder& pass_builder) {
            auto grid_desc               = GPUBufferDescriptor{};
            grid_desc.label              = "cluster_grid_buffer";
            grid_desc.size               = 16 * 9 * 24 * 2 * sizeof(uint32_t); // offset + count per cluster
            grid_desc.usage              = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;
            data.cluster_grid_buffer     = pass_builder.create<FrameGraphBuffer>(grid_desc);

            auto index_desc              = GPUBufferDescriptor{};
            index_desc.label             = "cluster_light_index_list";
            index_desc.size              = 16 * 9 * 24 * 64 * sizeof(uint32_t); // max 64 lights per cluster
            index_desc.usage             = GPUBufferUsage::STORAGE | GPUBufferUsage::COPY_DST;
            data.light_index_list        = pass_builder.create<FrameGraphBuffer>(index_desc);

            data.cluster_grid_buffer = pass_builder.write(data.cluster_grid_buffer);
            data.light_index_list    = pass_builder.write(data.light_index_list);
        },
        [](const LightCullingData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            // TODO: 1. Bind cluster building & light culling compute pipeline
            // TODO: 2. Dispatch compute workgroups over 3D cluster grid (16x9x24)
            // TODO: 3. Test light bounding spheres and cones against cluster AABB / frustum slice
            // TODO: 4. Write light indices and counts atomically into output storage buffers
        });
}
