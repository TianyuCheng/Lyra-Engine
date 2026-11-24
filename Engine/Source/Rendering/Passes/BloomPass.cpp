#include <algorithm>
#include <cmath>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Rendering/Passes/BloomPass.h>

using namespace lyra;
using namespace lyra::passes;

bool BloomPass::init(GPUDevice dev, Compiler compiler)
{
    if (initialized) {
        return true;
    }

    device = dev;

    // 1. compile compute shader module from sys://Shaders/PostProcess/Bloom.slang
    auto module = execute([&]() {
        auto desc   = CompileDescriptor{};
        desc.module = "bloom";
        desc.path   = "sys://Shaders/PostProcess/Bloom.slang";
        desc.source = nullptr;
        return compiler.compile(desc);
    });

    if (!module) {
        return false;
    }

    // 2. reflect downsample compute pipeline (cs_downsample)
    auto downsample_reflection = compiler.reflect({
        {*module, "cs_downsample"},
    });
    if (!downsample_reflection) {
        return false;
    }

    downsample_shader = execute([&]() {
        auto code  = module->get_shader_blob("cs_downsample");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "bloom_downsample_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });
    if (!downsample_shader.valid()) {
        return false;
    }

    downsample_blayouts.clear();
    for (auto& desc : downsample_reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        downsample_blayouts.push_back(blayout);
    }

    downsample_playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = downsample_blayouts;
        desc.immediate_size     = downsample_reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    downsample_pipeline = execute([&]() {
        auto cp_desc                = GPUComputePipelineDescriptor{};
        cp_desc.label               = "bloom_downsample_pipeline";
        cp_desc.layout              = downsample_playout;
        cp_desc.compute.module      = downsample_shader;
        cp_desc.compute.entry_point = "cs_downsample";
        return device.create_compute_pipeline(cp_desc);
    });

    // 3. reflect upsample compute pipeline (cs_upsample)
    auto upsample_reflection = compiler.reflect({
        {*module, "cs_upsample"},
    });
    if (!upsample_reflection) {
        return false;
    }

    upsample_shader = execute([&]() {
        auto code  = module->get_shader_blob("cs_upsample");
        auto desc  = GPUShaderModuleDescriptor{};
        desc.label = "bloom_upsample_shader";
        desc.data  = code->data;
        desc.size  = code->size;
        return device.create_shader_module(desc);
    });
    if (!upsample_shader.valid()) {
        return false;
    }

    upsample_blayouts.clear();
    for (auto& desc : upsample_reflection->get_bind_group_layouts()) {
        auto blayout = device.create_bind_group_layout(desc);
        upsample_blayouts.push_back(blayout);
    }

    upsample_playout = execute([&]() {
        auto desc               = GPUPipelineLayoutDescriptor{};
        desc.bind_group_layouts = upsample_blayouts;
        desc.immediate_size     = upsample_reflection->get_immediate_size();
        return device.create_pipeline_layout(desc);
    });

    upsample_pipeline = execute([&]() {
        auto cp_desc                = GPUComputePipelineDescriptor{};
        cp_desc.label               = "bloom_upsample_pipeline";
        cp_desc.layout              = upsample_playout;
        cp_desc.compute.module      = upsample_shader;
        cp_desc.compute.entry_point = "cs_upsample";
        return device.create_compute_pipeline(cp_desc);
    });

    // 4. create bilinear clamp-to-edge sampler
    linear_sampler = execute([&]() {
        auto desc           = GPUSamplerDescriptor{};
        desc.label          = "bloom_linear_sampler";
        desc.min_filter     = GPUFilterMode::LINEAR;
        desc.mag_filter     = GPUFilterMode::LINEAR;
        desc.mipmap_filter  = GPUMipmapFilterMode::LINEAR;
        desc.address_mode_u = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_v = GPUAddressMode::CLAMP_TO_EDGE;
        desc.address_mode_w = GPUAddressMode::CLAMP_TO_EDGE;
        return device.create_sampler(desc);
    });

    initialized = downsample_pipeline.valid() && upsample_pipeline.valid();
    return initialized;
}

void BloomPass::destroy()
{
    if (!initialized) {
        return;
    }

    device.wait();

    linear_sampler.destroy();
    upsample_pipeline.destroy();
    upsample_playout.destroy();
    upsample_shader.destroy();
    downsample_pipeline.destroy();
    downsample_playout.destroy();
    downsample_shader.destroy();

    downsample_blayouts.clear();
    upsample_blayouts.clear();
    initialized = false;
}

BloomData BloomPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> input_color,
    GPUExtent2D                         extent,
    const BloomSettings&                settings)
{
    return builder.add_pass<BloomData>(
        "BloomPass",
        [&](BloomData& data, FrameGraphPassBuilder& pass_builder) {
            data.input_color = pass_builder.sample(input_color);

            // compute 5 pyramid level dimensions starting at half resolution
            Array<GPUExtent2D, 5> mips;
            mips[0] = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)};
            for (usize i = 1; i < 5; ++i) {
                mips[i] = {std::max(1u, mips[i - 1].width / 2), std::max(1u, mips[i - 1].height / 2)};
            }

            // create downsample textures
            for (usize i = 0; i < 5; ++i) {
                auto desc            = GPUTextureDescriptor{};
                desc.label           = "bloom_down_mip";
                desc.size.width      = mips[i].width;
                desc.size.height     = mips[i].height;
                desc.size.depth      = 1;
                desc.dimension       = GPUTextureDimension::x2D;
                desc.format          = GPUTextureFormat::RGBA16FLOAT;
                desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
                desc.mip_level_count = 1;
                data.down_mips[i]    = pass_builder.create<FrameGraphTexture>(desc);
                data.down_mips[i]    = pass_builder.write(data.down_mips[i]);
            }

            // create upsample textures for levels 3, 2, 1, 0
            for (usize i = 0; i < 4; ++i) {
                auto desc            = GPUTextureDescriptor{};
                desc.label           = "bloom_up_mip";
                desc.size.width      = mips[i].width;
                desc.size.height     = mips[i].height;
                desc.size.depth      = 1;
                desc.dimension       = GPUTextureDimension::x2D;
                desc.format          = GPUTextureFormat::RGBA16FLOAT;
                desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
                desc.mip_level_count = 1;
                data.up_mips[i]      = pass_builder.create<FrameGraphTexture>(desc);
                data.up_mips[i]      = pass_builder.write(data.up_mips[i]);
            }

            // final bloom output is upsample level 0 (half resolution)
            data.bloom_output = data.up_mips[0];
        },
        [this, extent, settings](const BloomData& data, FrameGraphResources& resources, FrameGraphContext* ctx) {
            if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                return;
            }

            auto input_tex = resources.get<FrameGraphTexture>(data.input_color);
            if (!input_tex) {
                return;
            }

            // retrieve downsample and upsample textures
            Array<const FrameGraphTexture*, 5> down_texs;
            for (usize i = 0; i < 5; ++i) {
                down_texs[i] = resources.get<FrameGraphTexture>(data.down_mips[i]);
                if (!down_texs[i]) {
                    return;
                }
            }

            Array<const FrameGraphTexture*, 4> up_texs;
            for (usize i = 0; i < 4; ++i) {
                up_texs[i] = resources.get<FrameGraphTexture>(data.up_mips[i]);
                if (!up_texs[i]) {
                    return;
                }
            }

            // compute 5 pyramid level dimensions
            Array<GPUExtent2D, 5> mips;
            mips[0] = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)};
            for (usize i = 1; i < 5; ++i) {
                mips[i] = {std::max(1u, mips[i - 1].width / 2), std::max(1u, mips[i - 1].height / 2)};
            }

            auto command = ctx->cmdlist;

            // precompute threshold curve
            float   knee = std::clamp(settings.knee, 0.0f, 1.0f) * settings.threshold;
            Vector4 threshold_curve{
                settings.threshold,
                settings.threshold - knee,
                2.0f * knee,
                0.25f / std::max(knee, 1e-4f)};

            // =========================================================================
            // Stage 1: Downsample Pyramid (5 Dispatches)
            // =========================================================================
            command.set_pipeline(downsample_pipeline);

            for (usize i = 0; i < 5; ++i) {
                GPUExtent2D    src_ext  = (i == 0) ? extent : mips[i - 1];
                GPUExtent2D    dst_ext  = mips[i];
                GPUTextureView src_view = (i == 0) ? input_tex->view : down_texs[i - 1]->view;
                GPUTextureView dst_view = down_texs[i]->view;

                BloomPushConstants push{};
                push.inv_src_size    = Vector2(1.0f / static_cast<float>(src_ext.width), 1.0f / static_cast<float>(src_ext.height));
                push.inv_dst_size    = Vector2(1.0f / static_cast<float>(dst_ext.width), 1.0f / static_cast<float>(dst_ext.height));
                push.threshold       = (i == 0 && settings.enabled) ? threshold_curve : Vector4(0.0f, 0.0f, 0.0f, 0.0f);
                push.bloom_intensity = settings.intensity;
                push.upsample_radius = settings.upsample_radius;
                push.dst_width       = dst_ext.width;
                push.dst_height      = dst_ext.height;

                Array<GPUBindGroupEntry, 3> ds_entries{};
                ds_entries[0].type    = GPUResourceType::TEXTURE;
                ds_entries[0].binding = 0;
                ds_entries[0].texture = src_view;

                ds_entries[1].type    = GPUResourceType::SAMPLER;
                ds_entries[1].binding = 1;
                ds_entries[1].sampler = linear_sampler;

                ds_entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                ds_entries[2].binding = 2;
                ds_entries[2].texture = dst_view;

                auto bg_desc    = GPUBindGroupDescriptor{};
                bg_desc.layout  = downsample_blayouts[0];
                bg_desc.entries = ds_entries;
                auto bg         = ctx->allocate(bg_desc);

                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((dst_ext.width + 7) / 8, (dst_ext.height + 7) / 8, 1);
            }

            // =========================================================================
            // Stage 2: Upsample Pyramid (4 Dispatches: 4->3, 3->2, 2->1, 1->0)
            // =========================================================================
            command.set_pipeline(upsample_pipeline);

            for (int i = 3; i >= 0; --i) {
                usize       step    = static_cast<usize>(i);
                GPUExtent2D src_ext = mips[step + 1];
                GPUExtent2D dst_ext = mips[step];

                // source is either previous upsampled mip or the coarsest downsampled mip
                GPUTextureView src_view     = (step == 3) ? down_texs[4]->view : up_texs[step + 1]->view;
                GPUTextureView current_view = down_texs[step]->view;
                GPUTextureView dst_view     = up_texs[step]->view;

                BloomPushConstants push{};
                push.inv_src_size    = Vector2(1.0f / static_cast<float>(src_ext.width), 1.0f / static_cast<float>(src_ext.height));
                push.inv_dst_size    = Vector2(1.0f / static_cast<float>(dst_ext.width), 1.0f / static_cast<float>(dst_ext.height));
                push.threshold       = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
                push.bloom_intensity = settings.intensity;
                push.upsample_radius = settings.upsample_radius;
                push.dst_width       = dst_ext.width;
                push.dst_height      = dst_ext.height;

                Array<GPUBindGroupEntry, 4> us_entries{};
                us_entries[0].type    = GPUResourceType::TEXTURE;
                us_entries[0].binding = 0;
                us_entries[0].texture = src_view;

                us_entries[1].type    = GPUResourceType::SAMPLER;
                us_entries[1].binding = 1;
                us_entries[1].sampler = linear_sampler;

                us_entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                us_entries[2].binding = 2;
                us_entries[2].texture = dst_view;

                us_entries[3].type    = GPUResourceType::TEXTURE;
                us_entries[3].binding = 3;
                us_entries[3].texture = current_view;

                auto bg_desc    = GPUBindGroupDescriptor{};
                bg_desc.layout  = upsample_blayouts[0];
                bg_desc.entries = us_entries;
                auto bg         = ctx->allocate(bg_desc);

                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((dst_ext.width + 7) / 8, (dst_ext.height + 7) / 8, 1);
            }
        });
}
