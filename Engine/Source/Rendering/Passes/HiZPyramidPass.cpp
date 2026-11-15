#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/HiZPyramidPass.h>

using namespace lyra;
using namespace lyra::passes;

bool HiZPyramidPass::init(GPUDevice dev, Compiler compiler)
{
    if (initialized) {
        return true;
    }

    device = dev;

    // compile compute shader from sys://Shaders/GPUDriven/HiZ.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "hiz";
        desc.path   = "sys://Shaders/GPUDriven/HiZ.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    auto reflection = compiler.reflect({
        {*module, "csmain"},
    });

    if (!reflection) {
        return false;
    }

    cshader = execute([&]() {
        auto code  = module->get_shader_blob("csmain");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "hiz_compute_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });

    if (!cshader.valid()) {
        return false;
    }

    blayouts.clear();
    for (auto& desc : reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        blayouts.push_back(blayout);
    }

    playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = blayouts;
        desc.immediate_size     = reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    auto cp_desc                = GPUComputePipelineDescriptor{};
    cp_desc.label               = "hiz_pyramid_pipeline";
    cp_desc.layout              = playout;
    cp_desc.compute.module      = cshader;
    cp_desc.compute.entry_point = "csmain";
    pipeline                    = device.create_compute_pipeline(cp_desc);

    heap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 512;
        return device.create_bind_group_heap(desc);
    });

    initialized = pipeline.valid();
    return initialized;
}

void HiZPyramidPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    if (heap.valid()) {
        heap.destroy();
    }
    if (pipeline.valid()) {
        pipeline.destroy();
    }
    if (playout.valid()) {
        playout.destroy();
    }
    if (cshader.valid()) {
        cshader.destroy();
    }
    blayouts.clear();
    initialized = false;
}

HiZPyramidData HiZPyramidPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> depth_texture,
    GPUExtent2D                         depth_extent)
{
    return builder.add_pass<HiZPyramidData>(
        "HiZPyramidPass",
        [&](HiZPyramidData& data, FrameGraphPassBuilder& pass_builder) {
            data.depth_input = pass_builder.sample(depth_texture);

            // allocate mip-mapped single-channel R32FLOAT texture for hierarchical Z
            uint width  = std::max(1u, depth_extent.width / 2);
            uint height = std::max(1u, depth_extent.height / 2);
            uint mips   = static_cast<uint>(std::floor(std::log2(std::max(width, height)))) + 1;

            auto hiz_desc            = GPUTextureDescriptor{};
            hiz_desc.label           = "hiz_depth_pyramid";
            hiz_desc.size.width      = width;
            hiz_desc.size.height     = height;
            hiz_desc.size.depth      = 1;
            hiz_desc.dimension       = GPUTextureDimension::x2D;
            hiz_desc.format          = GPUTextureFormat::R32FLOAT;
            hiz_desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
            hiz_desc.mip_level_count = mips;

            data.hiz_texture = pass_builder.create<FrameGraphTexture>(hiz_desc);
            data.hiz_texture = pass_builder.write(data.hiz_texture);
        },
        [this, depth_extent](const HiZPyramidData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !pipeline.valid() || blayouts.empty()) {
                return;
            }

            auto depth_tex = resources.get<FrameGraphTexture>(data.depth_input);
            auto hiz_tex   = resources.get<FrameGraphTexture>(data.hiz_texture);
            if (!depth_tex || !hiz_tex) {
                return;
            }

            uint dst_w = std::max(1u, depth_extent.width / 2);
            uint dst_h = std::max(1u, depth_extent.height / 2);

            Array<GPUBindGroupEntry, 2> entries{};
            entries[0].type    = GPUResourceType::TEXTURE;
            entries[0].binding = 0;
            entries[0].texture = depth_tex->view;

            entries[1].type    = GPUResourceType::STORAGE_TEXTURE;
            entries[1].binding = 1;
            entries[1].texture = hiz_tex->view;

            auto bg_desc    = GPUBindGroupDescriptor{};
            bg_desc.heap    = heap;
            bg_desc.layout  = blayouts[0];
            bg_desc.entries = entries;
            auto bg         = device.create_bind_group(bg_desc);

            HiZPushConstants push{};
            push.src_width    = depth_extent.width;
            push.src_height   = depth_extent.height;
            push.dst_width    = dst_w;
            push.dst_height   = dst_h;
            push.is_reverse_z = 0;

            auto command = ctx->cmdlist;
            command.set_pipeline(pipeline);
            command.set_bind_group(0, bg);
            command.set_immediates(0, push);
            command.dispatch_workgroups((dst_w + 7) / 8, (dst_h + 7) / 8, 1);
        });
}
