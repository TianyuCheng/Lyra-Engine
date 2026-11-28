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
    

    // 1. downsample compute pipeline (cs_downsample)
    PipelineShaderDescriptor down_desc{};
    down_desc.module        = "sys://Shaders/PostProcess/Bloom.slang";
    down_desc.compute_entry = "cs_downsample";

    downsample_pipeline = ComputePipelineBuilder(device, compiler)
                              .with_shader(down_desc)
                              .with_label("bloom_downsample_pipeline")
                              .build();
    if (!downsample_pipeline.valid()) {
        return false;
    }

    // 2. upsample compute pipeline (cs_upsample)
    PipelineShaderDescriptor up_desc{};
    up_desc.module        = "sys://Shaders/PostProcess/Bloom.slang";
    up_desc.compute_entry = "cs_upsample";

    upsample_pipeline = ComputePipelineBuilder(device, compiler)
                            .with_shader(up_desc)
                            .with_label("bloom_upsample_pipeline")
                            .build();
    if (!upsample_pipeline.valid()) {
        return false;
    }

    // 3. create bilinear clamp-to-edge sampler
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
    downsample_pipeline.destroy();
    upsample_pipeline.destroy();
    initialized = false;
}

namespace
{
    struct BloomDownsamplePassData
    {
        FrameGraphHandle<FrameGraphTexture> src;
        FrameGraphHandle<FrameGraphTexture> dst;
    };

    struct BloomUpsamplePassData
    {
        FrameGraphHandle<FrameGraphTexture> src;
        FrameGraphHandle<FrameGraphTexture> curr;
        FrameGraphHandle<FrameGraphTexture> dst;
    };
} // namespace

BloomData BloomPass::add(
    FrameGraphBuilder&                  builder,
    FrameGraphHandle<FrameGraphTexture> input_color,
    GPUExtent2D                         extent,
    const BloomSettings&                settings)
{
    BloomData data{};
    data.input_color = input_color;

    // compute 5 pyramid level dimensions starting at half resolution
    Array<GPUExtent2D, 5> mips;
    mips[0] = {std::max(1u, extent.width / 2), std::max(1u, extent.height / 2)};
    for (usize i = 1; i < 5; ++i) {
        mips[i] = {std::max(1u, mips[i - 1].width / 2), std::max(1u, mips[i - 1].height / 2)};
    }

    // precompute threshold curve
    float   knee = std::clamp(settings.knee, 0.0f, 1.0f) * settings.threshold;
    Vector4 threshold_curve{
        settings.threshold,
        settings.threshold - knee,
        2.0f * knee,
        0.25f / std::max(knee, 1e-4f)};

    // =========================================================================
    // Stage 1: Downsample Pyramid (5 Passes)
    // =========================================================================
    for (usize i = 0; i < 5; ++i) {
        GPUExtent2D src_ext    = (i == 0) ? extent : mips[i - 1];
        GPUExtent2D dst_ext    = mips[i];
        auto        src_handle = (i == 0) ? input_color : data.down_mips[i - 1];

        String pass_name = "BloomDownsample_" + std::to_string(i);

        auto pass_data = builder.add_pass<BloomDownsamplePassData>(
            pass_name,
            [&, src_handle, dst_ext](BloomDownsamplePassData& pass_data, FrameGraphPassBuilder& pass_builder) {
                pass_data.src = pass_builder.sample(src_handle);

                auto desc            = GPUTextureDescriptor{};
                desc.label           = "bloom_down_mip";
                desc.size.width      = dst_ext.width;
                desc.size.height     = dst_ext.height;
                desc.size.depth      = 1;
                desc.dimension       = GPUTextureDimension::x2D;
                desc.format          = GPUTextureFormat::RGBA16FLOAT;
                desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
                desc.mip_level_count = 1;

                pass_data.dst = pass_builder.create<FrameGraphTexture>(desc);
                pass_data.dst = pass_builder.write(pass_data.dst);
            },
            [this, i, src_ext, dst_ext, threshold_curve, settings](const BloomDownsamplePassData& pass_data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                    return;
                }

                auto src_tex = resources.get<FrameGraphTexture>(pass_data.src);
                auto dst_tex = resources.get<FrameGraphTexture>(pass_data.dst);
                if (!src_tex || !dst_tex) {
                    return;
                }

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
                ds_entries[0].texture = src_tex->view;

                ds_entries[1].type    = GPUResourceType::SAMPLER;
                ds_entries[1].binding = 1;
                ds_entries[1].sampler = linear_sampler;

                ds_entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                ds_entries[2].binding = 2;
                ds_entries[2].texture = dst_tex->view;

                auto bg_desc    = GPUBindGroupDescriptor{};
                bg_desc.layout  = downsample_pipeline.layout.get_bind_group_layout(0);
                bg_desc.entries = ds_entries;
                auto bg         = ctx->allocate(bg_desc);

                auto command = ctx->cmdlist;
                command.set_pipeline(downsample_pipeline);
                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((dst_ext.width + 7) / 8, (dst_ext.height + 7) / 8, 1);
            });

        data.down_mips[i] = pass_data.dst;
    }

    // =========================================================================
    // Stage 2: Upsample Pyramid (4 Passes: 4->3, 3->2, 2->1, 1->0)
    // =========================================================================
    for (int i = 3; i >= 0; --i) {
        usize       step        = static_cast<usize>(i);
        GPUExtent2D src_ext     = mips[step + 1];
        GPUExtent2D dst_ext     = mips[step];
        auto        src_handle  = (step == 3) ? data.down_mips[4] : data.up_mips[step + 1];
        auto        curr_handle = data.down_mips[step];

        String pass_name = "BloomUpsample_" + std::to_string(step);

        auto pass_data = builder.add_pass<BloomUpsamplePassData>(
            pass_name,
            [&, src_handle, curr_handle, dst_ext](BloomUpsamplePassData& pass_data, FrameGraphPassBuilder& pass_builder) {
                pass_data.src  = pass_builder.sample(src_handle);
                pass_data.curr = pass_builder.sample(curr_handle);

                auto desc            = GPUTextureDescriptor{};
                desc.label           = "bloom_up_mip";
                desc.size.width      = dst_ext.width;
                desc.size.height     = dst_ext.height;
                desc.size.depth      = 1;
                desc.dimension       = GPUTextureDimension::x2D;
                desc.format          = GPUTextureFormat::RGBA16FLOAT;
                desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING;
                desc.mip_level_count = 1;

                pass_data.dst = pass_builder.create<FrameGraphTexture>(desc);
                pass_data.dst = pass_builder.write(pass_data.dst);
            },
            [this, step, src_ext, dst_ext, settings](const BloomUpsamplePassData& pass_data, FrameGraphResources& resources, FrameGraphContext* ctx) {
                if (!ctx || !ctx->cmdlist.handle.valid() || !valid()) {
                    return;
                }

                auto src_tex  = resources.get<FrameGraphTexture>(pass_data.src);
                auto curr_tex = resources.get<FrameGraphTexture>(pass_data.curr);
                auto dst_tex  = resources.get<FrameGraphTexture>(pass_data.dst);
                if (!src_tex || !curr_tex || !dst_tex) {
                    return;
                }

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
                us_entries[0].texture = src_tex->view;

                us_entries[1].type    = GPUResourceType::SAMPLER;
                us_entries[1].binding = 1;
                us_entries[1].sampler = linear_sampler;

                us_entries[2].type    = GPUResourceType::STORAGE_TEXTURE;
                us_entries[2].binding = 2;
                us_entries[2].texture = dst_tex->view;

                us_entries[3].type    = GPUResourceType::TEXTURE;
                us_entries[3].binding = 3;
                us_entries[3].texture = curr_tex->view;

                auto bg_desc    = GPUBindGroupDescriptor{};
                bg_desc.layout  = upsample_pipeline.layout.get_bind_group_layout(0);
                bg_desc.entries = us_entries;
                auto bg         = ctx->allocate(bg_desc);

                auto command = ctx->cmdlist;
                command.set_pipeline(upsample_pipeline);
                command.set_bind_group(0, bg);
                command.set_immediates(0, push);
                command.dispatch_workgroups((dst_ext.width + 7) / 8, (dst_ext.height + 7) / 8, 1);
            });

        data.up_mips[step] = pass_data.dst;
    }

    // final bloom output is upsample level 0 (half resolution)
    data.bloom_output = data.up_mips[0];
    return data;
}
