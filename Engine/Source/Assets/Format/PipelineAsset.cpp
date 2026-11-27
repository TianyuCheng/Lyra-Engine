#include <fstream>
#include <sstream>
#include <Lyra/FileSystem/VFSAPI.h>
#include <Lyra/Utilities/Enums.h>
#include <Lyra/Assets/Format/PipelineAsset.h>

using namespace lyra;

static GPUCompareFunction parse_compare_function(StringView str)
{
    if (str == ">=" || str == "greater_equal") return GPUCompareFunction::GREATER_EQUAL;
    if (str == "<=" || str == "less_equal") return GPUCompareFunction::LESS_EQUAL;
    if (str == "<" || str == "less") return GPUCompareFunction::LESS;
    if (str == ">" || str == "greater") return GPUCompareFunction::GREATER;
    if (str == "==" || str == "equal") return GPUCompareFunction::EQUAL;
    if (str == "!=" || str == "not_equal") return GPUCompareFunction::NOT_EQUAL;
    return parse_enum<GPUCompareFunction>(str).value_or(GPUCompareFunction::GREATER_EQUAL);
}

static Optional<GPUTextureFormat> parse_texture_format(StringView str)
{
    auto s = to_lower(str);
    if (s.empty() || s == "none" || s == "undefined") return std::nullopt;
    if (s == "d32_float" || s == "depth32_float") return GPUTextureFormat::DEPTH32FLOAT;
    if (s == "d16_unorm" || s == "depth16_unorm") return GPUTextureFormat::DEPTH16UNORM;
    return parse_enum<GPUTextureFormat>(str);
}

static GPUColorWriteFlags parse_write_mask(StringView str)
{
    auto s = to_lower(str);
    if (s == "all" || s == "color" || s == "rgba") return GPUColorWrite::ALL;
    if (s == "none" || s == "0") return GPUColorWriteFlags(0);

    GPUColorWriteFlags mask(0);
    if (s.find('r') != String::npos || s.find("red") != String::npos) mask |= GPUColorWrite::RED;
    if (s.find('g') != String::npos || s.find("green") != String::npos) mask |= GPUColorWrite::GREEN;
    if (s.find('b') != String::npos || s.find("blue") != String::npos) mask |= GPUColorWrite::BLUE;
    if (s.find('a') != String::npos || s.find("alpha") != String::npos) mask |= GPUColorWrite::ALPHA;
    return mask.value != 0 ? mask : GPUColorWrite::ALL;
}

GPUBlendState lyra::get_blend_preset(BlendPreset preset)
{
    auto blend = GPUBlendState{};
    switch (preset) {
        case BlendPreset::ALPHA:
            blend.color.src_factor = GPUBlendFactor::SRC_ALPHA;
            blend.color.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.color.operation  = GPUBlendOperation::ADD;
            blend.alpha.src_factor = GPUBlendFactor::ONE;
            blend.alpha.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.alpha.operation  = GPUBlendOperation::ADD;
            break;
        case BlendPreset::ADDITIVE:
            blend.color.src_factor = GPUBlendFactor::ONE;
            blend.color.dst_factor = GPUBlendFactor::ONE;
            blend.color.operation  = GPUBlendOperation::ADD;
            blend.alpha.src_factor = GPUBlendFactor::ONE;
            blend.alpha.dst_factor = GPUBlendFactor::ONE;
            blend.alpha.operation  = GPUBlendOperation::ADD;
            break;
        case BlendPreset::PREMULTIPLIED:
            blend.color.src_factor = GPUBlendFactor::ONE;
            blend.color.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.color.operation  = GPUBlendOperation::ADD;
            blend.alpha.src_factor = GPUBlendFactor::ONE;
            blend.alpha.dst_factor = GPUBlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.alpha.operation  = GPUBlendOperation::ADD;
            break;
        case BlendPreset::NONE:
        default:
            blend.color.src_factor = GPUBlendFactor::ONE;
            blend.color.dst_factor = GPUBlendFactor::ZERO;
            blend.color.operation  = GPUBlendOperation::ADD;
            blend.alpha.src_factor = GPUBlendFactor::ONE;
            blend.alpha.dst_factor = GPUBlendFactor::ZERO;
            blend.alpha.operation  = GPUBlendOperation::ADD;
            break;
    }
    return blend;
}

PipelineAsset PipelineAsset::from_toml(const TOML& table)
{
    PipelineAsset asset{};

    // 1. pipeline type
    auto type_str       = table["type"].value_or(table["pipeline_type"].value_or("render"));
    asset.pipeline_type = parse_enum<PipelineType>(type_str).value_or(PipelineType::RENDER);

    // 2. shader stage
    if (auto shader_tbl = table["shader"].as_table()) {
        asset.shader.module         = (*shader_tbl)["module"].value_or("");
        asset.shader.vertex_entry   = (*shader_tbl)["vertex_entry"].value_or("vsmain");
        asset.shader.fragment_entry = (*shader_tbl)["fragment_entry"].value_or("fsmain");
        asset.shader.compute_entry  = (*shader_tbl)["compute_entry"].value_or("csmain");

        if (auto macros_tbl = (*shader_tbl)["macros"].as_table()) {
            for (const auto& [k, v] : *macros_tbl) {
                asset.shader.macros[String(k.str())] = v.value_or("");
            }
        }
    } else if (auto compute_tbl = table["compute"].as_table()) {
        asset.pipeline_type        = PipelineType::COMPUTE;
        asset.shader.module        = (*compute_tbl)["module"].value_or("");
        asset.shader.compute_entry = (*compute_tbl)["entry"].value_or("csmain");
    }

    // 3. primitive / rasterizer state (default: TRIANGLE_LIST, BACK cull, CCW front-face)
    const auto prim_tbl = table["primitive"].as_table() ? table["primitive"].as_table() : table["rasterizer"].as_table();
    if (prim_tbl) {
        asset.primitive.topology =
            parse_enum<GPUPrimitiveTopology>((*prim_tbl)["topology"].value_or(""))
                .value_or(GPUPrimitiveTopology::TRIANGLE_LIST);
        asset.primitive.cull_mode =
            parse_enum<GPUCullMode>((*prim_tbl)["cull_mode"].value_or(""))
                .value_or(GPUCullMode::BACK);
        asset.primitive.front_face =
            parse_enum<GPUFrontFace>((*prim_tbl)["front_face"].value_or(""))
                .value_or(GPUFrontFace::CCW);
        asset.primitive.unclipped_depth = (*prim_tbl)["unclipped_depth"].value_or(false);
    }

    // 4. depth / stencil state (default: nullopt when omitted)
    if (auto ds_tbl = table["depth_stencil"].as_table()) {
        GPUDepthStencilState ds{};
        ds.depth_compare = parse_compare_function((*ds_tbl)["depth_compare"].value_or("greater_equal"));
        ds.depth_write_enabled =
            (*ds_tbl)["depth_write"].value_or((*ds_tbl)["depth_test"].value_or(true));
        ds.format     = parse_texture_format((*ds_tbl)["format"].value_or(""))
                            .value_or(GPUTextureFormat::DEPTH32FLOAT);
        ds.depth_bias = (*ds_tbl)["depth_bias"].value_or(0);
        ds.depth_bias_constant =
            static_cast<float>((*ds_tbl)["depth_bias_constant"].value_or(0.0));
        ds.depth_bias_slope_scale =
            static_cast<float>((*ds_tbl)["depth_bias_slope_scale"].value_or(0.0));
        ds.depth_bias_clamp =
            static_cast<float>((*ds_tbl)["depth_bias_clamp"].value_or(0.0));
        ds.stencil_read_mask =
            static_cast<GPUStencilValue>((*ds_tbl)["stencil_read_mask"].value_or(0xFFFFFFFF));
        ds.stencil_write_mask =
            static_cast<GPUStencilValue>((*ds_tbl)["stencil_write_mask"].value_or(0xFFFFFFFF));
        asset.depth_stencil = ds;
    }

    // 5. multisample state (default: 1 sample)
    if (auto ms_tbl = table["multisample"].as_table()) {
        asset.multisample.count                     = static_cast<GPUSize32>((*ms_tbl)["count"].value_or(1));
        asset.multisample.mask                      = static_cast<GPUSampleMask>((*ms_tbl)["mask"].value_or(0xFFFFFFFF));
        asset.multisample.alpha_to_coverage_enabled = (*ms_tbl)["alpha_to_coverage"].value_or(false);
        asset.multisample.alpha_to_one_enabled      = (*ms_tbl)["alpha_to_one"].value_or(false);
    }

    // 6. color targets & blending (default: 1 target RGBA8UNORM, no blend)
    if (auto targets_arr = table["targets"].as_array()) {
        for (const auto& elem : *targets_arr) {
            if (const auto target_tbl = elem.as_table()) {
                GPUColorTargetState target{};
                target.format     = parse_texture_format((*target_tbl)["format"].value_or("rgba8_unorm"))
                                        .value_or(GPUTextureFormat::RGBA8UNORM);
                target.write_mask = parse_write_mask((*target_tbl)["write_mask"].value_or("all"));

                auto preset_opt = parse_enum<BlendPreset>((*target_tbl)["blend"].value_or("none"));
                auto preset     = preset_opt.value_or(BlendPreset::NONE);
                if (preset != BlendPreset::NONE) {
                    target.blend        = get_blend_preset(preset);
                    target.blend_enable = true;
                } else {
                    target.blend        = get_blend_preset(BlendPreset::NONE);
                    target.blend_enable = (*target_tbl)["blend_enable"].value_or(false);
                }
                asset.targets.push_back(target);
            }
        }
    }

    // fallback: if render pipeline has no targets specified, populate single default color target
    if (asset.pipeline_type == PipelineType::RENDER && asset.targets.empty()) {
        GPUColorTargetState default_target{};
        default_target.format       = GPUTextureFormat::RGBA8UNORM;
        default_target.blend        = get_blend_preset(BlendPreset::NONE);
        default_target.write_mask   = GPUColorWrite::ALL;
        default_target.blend_enable = false;
        asset.targets.push_back(default_target);
    }

    return asset;
}

PipelineAsset PipelineAsset::deserialize_toml(StringView toml_text)
{
    if (toml_text.empty()) {
        return PipelineAsset{};
    }
    auto parsed = toml::parse(toml_text);
    return from_toml(parsed);
}

TOML PipelineAsset::to_toml() const
{
    TOML tbl;
    tbl.insert("type", to_string(pipeline_type));

    // shader section
    TOML shader_tbl;
    shader_tbl.insert("module", shader.module);
    if (pipeline_type == PipelineType::RENDER) {
        shader_tbl.insert("vertex_entry", shader.vertex_entry);
        shader_tbl.insert("fragment_entry", shader.fragment_entry);
    } else {
        shader_tbl.insert("compute_entry", shader.compute_entry);
    }
    if (!shader.macros.empty()) {
        TOML macros_tbl;
        for (const auto& [k, v] : shader.macros) {
            macros_tbl.insert(k, v);
        }
        shader_tbl.insert("macros", macros_tbl);
    }
    tbl.insert("shader", shader_tbl);

    if (pipeline_type == PipelineType::RENDER) {
        // primitive section
        TOML prim_tbl;
        prim_tbl.insert("topology", to_string(primitive.topology));
        prim_tbl.insert("cull_mode", to_string(primitive.cull_mode));
        prim_tbl.insert("front_face", to_string(primitive.front_face));
        if (primitive.unclipped_depth) {
            prim_tbl.insert("unclipped_depth", true);
        }
        tbl.insert("primitive", prim_tbl);

        // depth_stencil section
        if (depth_stencil.has_value()) {
            TOML ds_tbl;
            ds_tbl.insert("depth_write", depth_stencil->depth_write_enabled);
            ds_tbl.insert("depth_compare", to_string(depth_stencil->depth_compare));
            ds_tbl.insert("format", to_string(depth_stencil->format));
            if (depth_stencil->depth_bias != 0) {
                ds_tbl.insert("depth_bias", depth_stencil->depth_bias);
            }
            if (depth_stencil->depth_bias_constant != 0.0f) {
                ds_tbl.insert("depth_bias_constant", depth_stencil->depth_bias_constant);
            }
            if (depth_stencil->depth_bias_slope_scale != 0.0f) {
                ds_tbl.insert("depth_bias_slope_scale", depth_stencil->depth_bias_slope_scale);
            }
            if (depth_stencil->depth_bias_clamp != 0.0f) {
                ds_tbl.insert("depth_bias_clamp", depth_stencil->depth_bias_clamp);
            }
            tbl.insert("depth_stencil", ds_tbl);
        }

        // multisample section
        if (multisample.count > 1 || multisample.alpha_to_coverage_enabled || multisample.alpha_to_one_enabled) {
            TOML ms_tbl;
            ms_tbl.insert("count", static_cast<int64_t>(multisample.count));
            ms_tbl.insert("alpha_to_coverage", multisample.alpha_to_coverage_enabled);
            ms_tbl.insert("alpha_to_one", multisample.alpha_to_one_enabled);
            tbl.insert("multisample", ms_tbl);
        }

        // targets array
        toml::array targets_arr;
        for (const auto& t : targets) {
            TOML t_tbl;
            t_tbl.insert("format", to_string(t.format));
            t_tbl.insert("write_mask", to_string(t.write_mask));
            if (t.blend_enable) {
                t_tbl.insert("blend_enable", true);
            }
            targets_arr.push_back(t_tbl);
        }
        tbl.insert("targets", targets_arr);
    }

    return tbl;
}

String PipelineAsset::serialize_toml() const
{
    std::stringstream ss;
    ss << to_toml() << "\n";
    return ss.str();
}

GPURenderPipelineDescriptor PipelineAsset::to_render_descriptor(Vector<GPUColorTargetState>& target_storage) const
{
    GPURenderPipelineDescriptor desc{};
    desc.primitive = primitive;
    if (depth_stencil.has_value()) {
        desc.depth_stencil = *depth_stencil;
    }
    desc.multisample = multisample;
    target_storage   = targets;
    desc.fragment.targets =
        Span<GPUColorTargetState>(target_storage.data(), target_storage.size());
    return desc;
}

GPUComputePipelineDescriptor PipelineAsset::to_compute_descriptor() const
{
    GPUComputePipelineDescriptor desc{};
    return desc;
}

// ==============================================================================
// asset pipeline integration (loader, saver, cooker)
// ==============================================================================

static void* load_pipeline_asset(FileLoader* loader, FSPath path)
{
    auto content = loader->read<char>(path);
    if (content.empty()) {
        return new PipelineAsset{};
    }
    usize size = content.size();
    if (content.back() == '\0') {
        size--;
    }
    auto view = StringView(content.data(), size);
    return new PipelineAsset(PipelineAsset::deserialize_toml(view));
}

static uint get_pipeline_extensions(CString* extensions)
{
    if (extensions) {
        extensions[0] = ".pipeline";
    }
    return 1;
}

static bool pipeline_process(JSON& metadata, OSPath source_path, OSPath)
{
    metadata["path"] = reinterpret_cast<CString>(source_path);
    return true;
}

static bool save_pipeline_asset(const void* raw_asset, OSPath path)
{
    const auto asset = reinterpret_cast<const PipelineAsset*>(raw_asset);
    if (!asset) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << asset->serialize_toml();
    file.close();
    return !file.fail();
}

AssetLoaderAPI PipelineAsset::loader()
{
    auto api      = AssetLoaderAPI{};
    api.configure = nullptr;
    api.load      = load_pipeline_asset;
    api.unload    = [](void* asset) {
        delete reinterpret_cast<PipelineAsset*>(asset);
    };
    api.get_supported_extensions = get_pipeline_extensions;
    return api;
}

AssetCookerAPI PipelineAsset::cooker()
{
    auto api                     = AssetCookerAPI{};
    api.configure                = nullptr;
    api.process                  = pipeline_process;
    api.get_supported_extensions = get_pipeline_extensions;
    return api;
}

AssetSaverAPI PipelineAsset::saver()
{
    auto api                     = AssetSaverAPI{};
    api.configure                = nullptr;
    api.save                     = save_pipeline_asset;
    api.get_supported_extensions = get_pipeline_extensions;
    return api;
}
