#include "helper.h"
#include <filesystem>
#include <Lyra/Assets/Format/PipelineAsset.h>
#include <Lyra/FileSystem/VFSAPI.h>

namespace fs = std::filesystem;
using namespace lyra;

TEST_CASE("pipeline_serde::defaults_when_fields_absent")
{
    // minimal toml definition with almost all fields omitted
    StringView toml_src = R"""(
[shader]
module = "sys://Shaders/PostProcess/FXAA.slang"
)""";

    auto asset = PipelineAsset::deserialize_toml(toml_src);

    // verify sensible defaults
    CHECK_EQ(asset.pipeline_type, PipelineType::RENDER);
    CHECK_EQ(asset.shader.module, "sys://Shaders/PostProcess/FXAA.slang");
    CHECK_EQ(asset.shader.vertex_entry, "vsmain");
    CHECK_EQ(asset.shader.fragment_entry, "fsmain");
    CHECK_EQ(asset.shader.compute_entry, "csmain");
    CHECK(asset.shader.macros.empty());

    // primitive defaults
    CHECK_EQ(asset.primitive.topology, GPUPrimitiveTopology::TRIANGLE_LIST);
    CHECK_EQ(asset.primitive.cull_mode, GPUCullMode::BACK);
    CHECK_EQ(asset.primitive.front_face, GPUFrontFace::CCW);
    CHECK_FALSE(asset.primitive.unclipped_depth);

    // depth stencil default: nullopt when omitted
    CHECK_FALSE(asset.depth_stencil.has_value());

    // multisample defaults
    CHECK_EQ(asset.multisample.count, 1u);
    CHECK_EQ(asset.multisample.mask, 0xFFFFFFFFu);
    CHECK_FALSE(asset.multisample.alpha_to_coverage_enabled);

    // single default target populated
    REQUIRE_EQ(asset.targets.size(), 1u);
    CHECK_EQ(asset.targets[0].format, GPUTextureFormat::RGBA8UNORM);
    CHECK_EQ(asset.targets[0].write_mask, GPUColorWrite::ALL);
    CHECK_FALSE(asset.targets[0].blend_enable);

    // verify rhi descriptor conversion
    Vector<GPUColorTargetState> target_storage;
    auto                        desc = asset.to_render_descriptor(target_storage);
    CHECK_EQ(desc.primitive.topology, GPUPrimitiveTopology::TRIANGLE_LIST);
    CHECK_EQ(desc.primitive.cull_mode, GPUCullMode::BACK);
    CHECK_EQ(desc.primitive.front_face, GPUFrontFace::CCW);
    REQUIRE_EQ(desc.fragment.targets.size(), 1u);
    CHECK_EQ(desc.fragment.targets[0].format, GPUTextureFormat::RGBA8UNORM);
    CHECK_FALSE(desc.fragment.targets[0].blend_enable);
}

TEST_CASE("pipeline_serde::roundtrip_render_pipeline")
{
    PipelineAsset original{};
    original.pipeline_type                   = PipelineType::RENDER;
    original.shader.module                   = "sys://Shaders/Deferred/GBuffer.slang";
    original.shader.vertex_entry             = "vs_main";
    original.shader.fragment_entry           = "fs_main";
    original.shader.macros["HAS_NORMAL_MAP"] = "1";
    original.shader.macros["SHADOW_PCF"]     = "16";

    original.primitive.topology        = GPUPrimitiveTopology::TRIANGLE_LIST;
    original.primitive.cull_mode       = GPUCullMode::FRONT;
    original.primitive.front_face      = GPUFrontFace::CW;
    original.primitive.unclipped_depth = true;

    GPUDepthStencilState ds{};
    ds.depth_write_enabled    = true;
    ds.depth_compare          = GPUCompareFunction::GREATER_EQUAL;
    ds.format                 = GPUTextureFormat::DEPTH32FLOAT;
    ds.depth_bias             = 2;
    ds.depth_bias_constant    = 1.5f;
    ds.depth_bias_slope_scale = 2.0f;
    ds.depth_bias_clamp       = 0.1f;
    original.depth_stencil    = ds;

    original.multisample.count                     = 4;
    original.multisample.alpha_to_coverage_enabled = true;

    // target 0: hdr albedo
    GPUColorTargetState t0{};
    t0.format       = GPUTextureFormat::RGBA16FLOAT;
    t0.blend        = get_blend_preset(BlendPreset::NONE);
    t0.write_mask   = GPUColorWrite::ALL;
    t0.blend_enable = false;
    original.targets.push_back(t0);

    // target 1: normals with additive blend
    GPUColorTargetState t1{};
    t1.format       = GPUTextureFormat::RGBA16SNORM;
    t1.blend        = get_blend_preset(BlendPreset::ADDITIVE);
    t1.blend_enable = true;
    t1.write_mask   = GPUColorWrite::RED | GPUColorWrite::GREEN | GPUColorWrite::BLUE;
    original.targets.push_back(t1);

    // serialize to toml
    String toml_str = original.serialize_toml();
    CHECK_FALSE(toml_str.empty());

    // deserialize back
    auto loaded = PipelineAsset::deserialize_toml(toml_str);

    CHECK_EQ(loaded.pipeline_type, PipelineType::RENDER);
    CHECK_EQ(loaded.shader.module, original.shader.module);
    CHECK_EQ(loaded.shader.vertex_entry, "vs_main");
    CHECK_EQ(loaded.shader.fragment_entry, "fs_main");
    CHECK_EQ(loaded.shader.macros.size(), 2u);
    CHECK_EQ(loaded.shader.macros.at("HAS_NORMAL_MAP"), "1");
    CHECK_EQ(loaded.shader.macros.at("SHADOW_PCF"), "16");

    CHECK_EQ(loaded.primitive.topology, GPUPrimitiveTopology::TRIANGLE_LIST);
    CHECK_EQ(loaded.primitive.cull_mode, GPUCullMode::FRONT);
    CHECK_EQ(loaded.primitive.front_face, GPUFrontFace::CW);
    CHECK(loaded.primitive.unclipped_depth);

    REQUIRE(loaded.depth_stencil.has_value());
    CHECK(loaded.depth_stencil->depth_write_enabled);
    CHECK_EQ(loaded.depth_stencil->depth_compare, GPUCompareFunction::GREATER_EQUAL);
    CHECK_EQ(loaded.depth_stencil->format, GPUTextureFormat::DEPTH32FLOAT);
    CHECK_EQ(loaded.depth_stencil->depth_bias, 2);
    CHECK_EQ(loaded.depth_stencil->depth_bias_constant, doctest::Approx(1.5f));
    CHECK_EQ(loaded.depth_stencil->depth_bias_slope_scale, doctest::Approx(2.0f));
    CHECK_EQ(loaded.depth_stencil->depth_bias_clamp, doctest::Approx(0.1f));

    CHECK_EQ(loaded.multisample.count, 4u);
    CHECK(loaded.multisample.alpha_to_coverage_enabled);

    REQUIRE_EQ(loaded.targets.size(), 2u);
    CHECK_EQ(loaded.targets[0].format, GPUTextureFormat::RGBA16FLOAT);
    CHECK_FALSE(loaded.targets[0].blend_enable);

    CHECK_EQ(loaded.targets[1].format, GPUTextureFormat::RGBA16SNORM);
    CHECK(loaded.targets[1].blend_enable);
    CHECK(loaded.targets[1].write_mask.contains(GPUColorWrite::RED));
    CHECK(loaded.targets[1].write_mask.contains(GPUColorWrite::GREEN));
    CHECK(loaded.targets[1].write_mask.contains(GPUColorWrite::BLUE));
    CHECK_FALSE(loaded.targets[1].write_mask.contains(GPUColorWrite::ALPHA));
}

TEST_CASE("pipeline_serde::compute_pipeline")
{
    StringView toml_src = R"""(
type = "compute"

[shader]
module = "sys://Shaders/Compute/LightCulling.slang"
compute_entry = "cull_lights"
)""";

    auto asset = PipelineAsset::deserialize_toml(toml_src);

    CHECK_EQ(asset.pipeline_type, PipelineType::COMPUTE);
    CHECK_EQ(asset.shader.module, "sys://Shaders/Compute/LightCulling.slang");
    CHECK_EQ(asset.shader.compute_entry, "cull_lights");

    auto toml_out = asset.serialize_toml();
    auto reloaded = PipelineAsset::deserialize_toml(toml_out);

    CHECK_EQ(reloaded.pipeline_type, PipelineType::COMPUTE);
    CHECK_EQ(reloaded.shader.module, "sys://Shaders/Compute/LightCulling.slang");
    CHECK_EQ(reloaded.shader.compute_entry, "cull_lights");
}

TEST_CASE("pipeline_serde::blend_presets")
{
    // alpha blend preset
    auto alpha_blend = get_blend_preset(BlendPreset::ALPHA);
    CHECK_EQ(alpha_blend.color.src_factor, GPUBlendFactor::SRC_ALPHA);
    CHECK_EQ(alpha_blend.color.dst_factor, GPUBlendFactor::ONE_MINUS_SRC_ALPHA);
    CHECK_EQ(alpha_blend.color.operation, GPUBlendOperation::ADD);
    CHECK_EQ(alpha_blend.alpha.src_factor, GPUBlendFactor::ONE);
    CHECK_EQ(alpha_blend.alpha.dst_factor, GPUBlendFactor::ONE_MINUS_SRC_ALPHA);

    // additive blend preset
    auto add_blend = get_blend_preset(BlendPreset::ADDITIVE);
    CHECK_EQ(add_blend.color.src_factor, GPUBlendFactor::ONE);
    CHECK_EQ(add_blend.color.dst_factor, GPUBlendFactor::ONE);
    CHECK_EQ(add_blend.color.operation, GPUBlendOperation::ADD);

    // premultiplied blend preset
    auto premul_blend = get_blend_preset(BlendPreset::PREMULTIPLIED);
    CHECK_EQ(premul_blend.color.src_factor, GPUBlendFactor::ONE);
    CHECK_EQ(premul_blend.color.dst_factor, GPUBlendFactor::ONE_MINUS_SRC_ALPHA);
}

TEST_CASE("pipeline_serde::file_io_saver_and_loader")
{
    auto temp_dir = fs::temp_directory_path() / "lyra_pipe_serde_test";
    fs::create_directories(temp_dir);

    auto file_path = temp_dir / "test_fxaa.pipeline";

    PipelineAsset original{};
    original.shader.module       = "sys://Shaders/PostProcess/FXAA.slang";
    original.primitive.cull_mode = GPUCullMode::NONE;
    original.depth_stencil       = std::nullopt;

    GPUColorTargetState target{};
    target.format       = GPUTextureFormat::RGBA8UNORM;
    target.blend        = get_blend_preset(BlendPreset::NONE);
    target.blend_enable = false;
    original.targets.push_back(target);

    // save to disk using assetsaverapi
    bool saved = PipelineAsset::saver().save(&original, file_path.c_str());
    REQUIRE(saved);

    // load back using fileloader and assetloaderapi
    FileLoader loader(FSLoader::NATIVE);
    loader.mount("app://", temp_dir.c_str(), 0);

    auto loader_api = PipelineAsset::loader();
    auto loaded     = static_cast<PipelineAsset*>(loader_api.load(&loader, "app://test_fxaa.pipeline"));
    REQUIRE_NE(loaded, nullptr);

    CHECK_EQ(loaded->shader.module, "sys://Shaders/PostProcess/FXAA.slang");
    CHECK_EQ(loaded->primitive.cull_mode, GPUCullMode::NONE);
    CHECK_FALSE(loaded->depth_stencil.has_value());
    REQUIRE_EQ(loaded->targets.size(), 1u);
    CHECK_EQ(loaded->targets[0].format, GPUTextureFormat::RGBA8UNORM);

    loader_api.unload(loaded);
    fs::remove_all(temp_dir);
}
