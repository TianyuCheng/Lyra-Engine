#include <algorithm>

#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/MeshPass.h>

using namespace lyra;

MeshPass::~MeshPass()
{
    destroy();
}

bool MeshPass::init(GPUDevice dev, GPUBindGroupLayoutHandle in_vertex_layout, Ref<GeometryBindGroupCache> cache)
{
    if (initialized) {
        destroy();
    }

    device        = dev;
    vertex_layout = in_vertex_layout;

    if (cache) {
        geometry_cache = cache;
        if (vertex_layout.valid() && (!geometry_cache->get_layout().valid() || !geometry_cache->valid())) {
            geometry_cache->init(device, vertex_layout);
        }
    } else {
        geometry_cache = make_ref<GeometryBindGroupCache>();
        geometry_cache->init(device, vertex_layout);
    }

    initialized = geometry_cache && geometry_cache->valid();
    return initialized;
}

void MeshPass::destroy()
{
    if (!initialized) {
        return;
    }

    geometry_cache.reset();
    batches.clear();
    draw_calls         = 0;
    triangles_rendered = 0;
    initialized        = false;
}

void MeshPass::set_geometry_cache(Ref<GeometryBindGroupCache> cache)
{
    geometry_cache = cache;
}

void MeshPass::clear()
{
    batches.clear();
    draw_calls         = 0;
    triangles_rendered = 0;
}

void MeshPass::prepare(
    GPUDevice          dev,
    const RenderScene& scene,
    const RenderView&  view,
    PipelineResolver   resolve_pipeline)
{
    clear();

    if (scene.meshes.empty()) {
        return;
    }

    struct SortableMesh
    {
        RenderSortKey     sort_key;
        GPURenderPipeline pipeline;
        GPUBufferHandle   vertex_buffer;
        GPUBindGroup      vertex_bind_group;
        MeshDrawCommand   cmd;
    };

    Vector<SortableMesh> sortable_items;
    sortable_items.reserve(scene.meshes.size());

    for (const auto& mesh : scene.meshes) {
        if (!mesh.geometry.valid() || mesh.index_count == 0) {
            continue;
        }

        GPUBufferHandle vb_handle = mesh.geometry.vertex_slice.buffer;
        if (!vb_handle.valid()) {
            continue;
        }

        auto pipeline = resolve_pipeline(mesh);
        if (!pipeline.valid()) {
            continue;
        }

        Vector3 mesh_center = (mesh.world_bounds.min + mesh.world_bounds.max) * 0.5f;
        float   depth       = glm::length(view.camera_position - mesh_center);

        RenderSortKey key = RenderSortKey::make(static_cast<uint>(pipeline.handle.value), static_cast<uint>(vb_handle.value), depth);

        const auto& offsets     = mesh.geometry.stream_offsets;
        GPUSize64   base_offset = mesh.geometry.vertex_slice.offset;

        MeshDrawInstance inst{};
        inst.model           = mesh.transform;
        inst.base_color      = mesh.material_params.base_color;
        inst.roughness       = mesh.material_params.roughness;
        inst.metallic        = mesh.material_params.metallic;
        inst.clear_coat      = mesh.material_params.clear_coat;
        inst.material_id     = mesh.material_id;
        inst.position_offset = static_cast<uint>(base_offset + (offsets.has_position() ? offsets.position : 0));
        inst.normal_offset   = static_cast<uint>(base_offset + (offsets.has_normal() ? offsets.normal : inst.position_offset));
        inst.uv_offset       = static_cast<uint>(base_offset + (offsets.has_texcoord0() ? offsets.texcoord0 : inst.position_offset));

        MeshDrawCommand cmd{};
        cmd.instance     = inst;
        cmd.index_buffer = mesh.geometry.index_slice.buffer;
        cmd.index_format = mesh.geometry.index_format;
        cmd.index_offset = mesh.geometry.index_slice.offset;
        cmd.index_count  = mesh.index_count;
        cmd.first_index  = mesh.first_index;

        sortable_items.push_back(SortableMesh{
            .sort_key          = key,
            .pipeline          = pipeline,
            .vertex_buffer     = vb_handle,
            .vertex_bind_group = mesh.geometry.vertex_bind_group,
            .cmd               = cmd,
        });

        draw_calls++;
        triangles_rendered += mesh.index_count / 3;
    }

    if (sortable_items.empty()) {
        return;
    }

    std::sort(sortable_items.begin(), sortable_items.end(), [](const SortableMesh& a, const SortableMesh& b) {
        return a.sort_key < b.sort_key;
    });

    MeshDrawBatch*    current_batch = nullptr;
    GPURenderPipeline last_pipe{};
    GPUBufferHandle   last_vbuf{};

    for (const auto& item : sortable_items) {
        if (!current_batch || item.pipeline.handle != last_pipe.handle || item.vertex_buffer.value != last_vbuf.value) {
            batches.push_back(MeshDrawBatch{
                .pipeline          = item.pipeline,
                .vertex_buffer     = item.vertex_buffer,
                .vertex_bind_group = {},
                .commands          = {},
            });
            current_batch = &batches.back();
            last_pipe     = item.pipeline;
            last_vbuf     = item.vertex_buffer;

            if (item.vertex_bind_group.valid()) {
                current_batch->vertex_bind_group = item.vertex_bind_group;
            } else if (geometry_cache) {
                current_batch->vertex_bind_group = geometry_cache->get_or_create(item.vertex_buffer);
            }
        }

        current_batch->commands.push_back(item.cmd);
    }
}

void MeshPass::draw(GPUCommandEncoder& command, GPUBindGroup pass_bind_group) const
{
    GPURenderPipeline last_pipe{};
    GPUBufferHandle   last_vbuf{};

    for (const auto& batch : batches) {
        if (batch.pipeline.handle != last_pipe.handle) {
            command.set_pipeline(batch.pipeline);
            last_pipe = batch.pipeline;

            if (pass_bind_group.valid()) {
                command.set_bind_group(0, pass_bind_group);
            }
        }

        if (batch.vertex_bind_group.valid() && batch.vertex_buffer.value != last_vbuf.value) {
            command.set_bind_group(1, batch.vertex_bind_group);
            last_vbuf = batch.vertex_buffer;
        }

        for (const auto& cmd : batch.commands) {
            command.set_immediates(0, cmd.instance);

            GPUBuffer ibuf{};
            ibuf.handle = cmd.index_buffer;
            command.set_index_buffer(ibuf, cmd.index_format, cmd.index_offset);
            command.draw_indexed(cmd.index_count, 1, cmd.first_index, 0, 0);
        }
    }
}
