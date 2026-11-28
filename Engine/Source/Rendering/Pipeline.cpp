#include <Lyra/Rendering/Pipeline.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

#pragma region PipelineReflection
Result<ReflectedPipelineLayout> PipelineReflection::reflect_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc,
    View<ShaderEntryPoint>          extra_entries)
{
    CompileDescriptor desc{};
    desc.path    = shader_desc.module.c_str();
    desc.defines = shader_desc.defines;

    auto module = compiler.compile(desc);
    if (!module) {
        return tl::make_unexpected("reflection failed, cannot compile module '" + shader_desc.module + "': " + module.error());
    }

    Vector<ShaderEntryPoint> entries;
    if (!shader_desc.vertex_entry.empty() && !shader_desc.fragment_entry.empty()) {
        entries.push_back({**module, shader_desc.vertex_entry.c_str()});
        entries.push_back({**module, shader_desc.fragment_entry.c_str()});
    } else if (!shader_desc.compute_entry.empty()) {
        entries.push_back({**module, shader_desc.compute_entry.c_str()});
    } else if (!shader_desc.vertex_entry.empty()) {
        entries.push_back({**module, shader_desc.vertex_entry.c_str()});
    }
    for (const auto& extra : extra_entries) {
        entries.push_back(extra);
    }

    auto reflection = compiler.reflect(entries);
    if (!reflection) {
        return tl::make_unexpected("compiler reflection query failed for " + shader_desc.module + ": " + reflection.error());
    }

    ReflectedPipelineLayout result{};

    auto bg_layouts = (*reflection)->get_bind_group_layouts();
    if (!bg_layouts) {
        return tl::make_unexpected("failed to get bind group layouts: " + bg_layouts.error());
    }
    result.bind_group_descriptors.assign(bg_layouts->begin(), bg_layouts->end());
    result.immediate_size = (*reflection)->get_immediate_size().value_or(0);

    Vector<GPUBindGroupLayoutHandle> bgl_handles;
    for (usize i = 0; i < result.bind_group_descriptors.size(); ++i) {
        const auto& bg_desc = result.bind_group_descriptors[i];
        auto        bgl     = device.create_bind_group_layout(bg_desc);
        if (!bgl.valid()) {
            result.destroy();
            return tl::make_unexpected("failed to create bind group layout index " + std::to_string(i));
        }

        result.bind_group_layouts.push_back(bgl);
        bgl_handles.push_back(bgl.handle);

        if (bg_desc.label) {
            result.bind_group_locations[bg_desc.label] = static_cast<uint>(i);
        }
    }

    GPUPipelineLayoutDescriptor playout_desc{};
    playout_desc.bind_group_layouts = Span<GPUBindGroupLayoutHandle>(bgl_handles.data(), bgl_handles.size());
    playout_desc.immediate_size     = result.immediate_size;

    result.layout = device.create_pipeline_layout(playout_desc);
    if (!result.layout.valid()) {
        result.destroy();
        return tl::make_unexpected("failed to create hardware pipeline layout for " + shader_desc.module);
    }

    return result;
}

Result<ReflectedPipelineLayout> PipelineReflection::reflect_render_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc)
{
    PipelineShaderDescriptor render_shader = shader_desc;
    render_shader.compute_entry.clear();
    return reflect_layout(device, compiler, loader, render_shader);
}

Result<ReflectedPipelineLayout> PipelineReflection::reflect_compute_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc)
{
    PipelineShaderDescriptor compute_shader = shader_desc;
    compute_shader.vertex_entry.clear();
    compute_shader.fragment_entry.clear();
    return reflect_layout(device, compiler, loader, compute_shader);
}
#pragma endregion PipelineReflection

#pragma region RenderPipelineBuilder
RenderPipelineBuilder::RenderPipelineBuilder(GPUDevice dev, Compiler comp, FileLoader* loader)
    : device(dev), compiler(comp), loader(loader)
{
    asset.pipeline_type = PipelineType::RENDER;
}

RenderPipelineBuilder& RenderPipelineBuilder::from_asset(const PipelineAsset& a)
{
    asset          = a;
    custom_targets = !a.targets.empty();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::from_asset(StringView virtual_path)
{
    if (!loader) {
        error_message = "no FileLoader available to read pipeline asset: " + String(virtual_path);
        return *this;
    }
    auto data = loader->read<char>(String(virtual_path).c_str());
    if (!data || data->empty()) {
        error_message = "failed to read pipeline asset file: " + String(virtual_path);
        return *this;
    }
    String text(data->data(), data->size());
    asset          = PipelineAsset::deserialize_toml(text);
    custom_targets = !asset.targets.empty();
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_shader(const PipelineShaderDescriptor& shader)
{
    asset.shader = shader;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_shader_module(StringView module_path)
{
    asset.shader.module = String(module_path);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_vertex_entry(StringView entry)
{
    asset.shader.vertex_entry = String(entry);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_fragment_entry(StringView entry)
{
    asset.shader.fragment_entry = String(entry);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_macro(StringView key, StringView value)
{
    return with_define(key, value);
}

RenderPipelineBuilder& RenderPipelineBuilder::with_define(StringView key, StringView value)
{
    for (auto& def : asset.shader.defines) {
        if (def.key == key) {
            def.value = String(value);
            return *this;
        }
    }
    asset.shader.defines.push_back(ShaderDefine{String(key), String(value)});
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_defines(View<ShaderDefine> defs)
{
    for (const auto& def : defs) {
        with_define(def.key, def.value);
    }
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_vertex_layouts(View<GPUVertexBufferLayout> layouts)
{
    vertex_layouts.assign(layouts.begin(), layouts.end());
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_vertex_layout(const GPUVertexBufferLayout& layout)
{
    vertex_layouts.push_back(layout);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_primitive(const GPUPrimitiveState& primitive)
{
    asset.primitive = primitive;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_topology(GPUPrimitiveTopology topology)
{
    asset.primitive.topology = topology;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_cull_mode(GPUCullMode cull_mode)
{
    asset.primitive.cull_mode = cull_mode;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_front_face(GPUFrontFace front_face)
{
    asset.primitive.front_face = front_face;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_depth_stencil(const GPUDepthStencilState& ds)
{
    asset.depth_stencil = ds;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_depth_stencil(
    GPUTextureFormat   format,
    bool               write_enabled,
    GPUCompareFunction compare)
{
    GPUDepthStencilState ds{};
    ds.format              = format;
    ds.depth_write_enabled = write_enabled;
    ds.depth_compare       = compare;
    asset.depth_stencil    = ds;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::without_depth_stencil()
{
    asset.depth_stencil = std::nullopt;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_multisample(const GPUMultisampleState& ms)
{
    asset.multisample = ms;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_targets(View<GPUColorTargetState> targets)
{
    asset.targets.assign(targets.begin(), targets.end());
    custom_targets = true;
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_target(const GPUColorTargetState& target)
{
    if (!custom_targets) {
        asset.targets.clear();
        custom_targets = true;
    }
    asset.targets.push_back(target);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_target(
    GPUTextureFormat   format,
    BlendPreset        blend,
    GPUColorWriteFlags write_mask)
{
    GPUColorTargetState target{};
    target.format       = format;
    target.blend        = get_blend_preset(blend);
    target.blend_enable = (blend != BlendPreset::NONE);
    target.write_mask   = write_mask;
    return with_target(target);
}

RenderPipelineBuilder& RenderPipelineBuilder::with_label(CString l)
{
    label = l;
    return *this;
}

PipelineBundle RenderPipelineBuilder::build()
{
    if (error_message) {
        spdlog::error("RenderPipelineBuilder: {}", *error_message);
        return {};
    }

    // 1. synthesize pipeline layout directly from Slang reflection
    auto layout = PipelineReflection::reflect_render_layout(device, compiler, loader, asset.shader);
    if (!layout) {
        spdlog::error("RenderPipelineBuilder: reflection failed for '{}': {}", asset.shader.module, layout.error());
        return {};
    }

    // 2. compile shader module once
    CompileDescriptor cdesc{};
    cdesc.path    = asset.shader.module.c_str();
    cdesc.defines = asset.shader.defines;

    auto compiled_module = compiler.compile(cdesc);
    if (!compiled_module) {
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: failed to compile shader module '{}': {}", asset.shader.module, compiled_module.error());
        return {};
    }

    auto vblob = (*compiled_module)->get_shader_blob(asset.shader.vertex_entry.c_str());
    if (!vblob || !vblob->data || vblob->size == 0) {
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: failed to get shader blob for vertex entry '{}'", asset.shader.vertex_entry);
        return {};
    }

    GPUShaderModuleDescriptor vsm_desc{};
    auto                      vlabel = asset.shader.module + ":" + asset.shader.vertex_entry;
    vsm_desc.label                   = vlabel.c_str();
    vsm_desc.data                    = vblob->data;
    vsm_desc.size                    = vblob->size;
    auto vshader                     = device.create_shader_module(vsm_desc);
    if (!vshader.valid()) {
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: device failed to create vertex shader module: {}", vlabel);
        return {};
    }

    auto fblob = (*compiled_module)->get_shader_blob(asset.shader.fragment_entry.c_str());
    if (!fblob || !fblob->data || fblob->size == 0) {
        vshader.destroy();
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: failed to get shader blob for fragment entry '{}'", asset.shader.fragment_entry);
        return {};
    }

    GPUShaderModuleDescriptor fsm_desc{};
    auto                      flabel = asset.shader.module + ":" + asset.shader.fragment_entry;
    fsm_desc.label                   = flabel.c_str();
    fsm_desc.data                    = fblob->data;
    fsm_desc.size                    = fblob->size;
    auto fshader                     = device.create_shader_module(fsm_desc);
    if (!fshader.valid()) {
        vshader.destroy();
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: device failed to create fragment shader module: {}", flabel);
        return {};
    }

    // 3. construct GPURenderPipelineDescriptor
    Vector<GPUColorTargetState> target_storage;
    auto                        pdesc = asset.to_render_descriptor(target_storage);

    pdesc.layout               = layout->layout.handle;
    pdesc.vertex.module        = vshader.handle;
    pdesc.vertex.entry_point   = asset.shader.vertex_entry.c_str();
    pdesc.vertex.buffers       = vertex_layouts;
    pdesc.fragment.module      = fshader.handle;
    pdesc.fragment.entry_point = asset.shader.fragment_entry.c_str();
    pdesc.label                = label ? label : asset.shader.module.c_str();

    auto pipeline = device.create_render_pipeline(pdesc);

    if (!pipeline.valid()) {
        vshader.destroy();
        fshader.destroy();
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: device failed to create render pipeline: {}", pdesc.label ? pdesc.label : "(unlabeled)");
        return {};
    }

    Vector<GPUShaderModule> sms;
    sms.push_back(vshader);
    sms.push_back(fshader);

    return PipelineBundle{
        .pipeline       = pipeline,
        .layout         = std::move(*layout),
        .shader_modules = std::move(sms),
    };
}
#pragma endregion RenderPipelineBuilder

#pragma region ComputePipelineBuilder

ComputePipelineBuilder::ComputePipelineBuilder(GPUDevice dev, Compiler comp, FileLoader* loader)
    : device(dev), compiler(comp), loader(loader)
{
    asset.pipeline_type = PipelineType::COMPUTE;
}

ComputePipelineBuilder& ComputePipelineBuilder::from_asset(const PipelineAsset& a)
{
    asset = a;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::from_asset(StringView virtual_path)
{
    if (!loader) {
        error_message = "no FileLoader available to read pipeline asset: " + String(virtual_path);
        return *this;
    }
    auto data = loader->read<char>(String(virtual_path).c_str());
    if (!data || data->empty()) {
        error_message = "failed to read pipeline asset file: " + String(virtual_path);
        return *this;
    }
    String text(data->data(), data->size());
    asset = PipelineAsset::deserialize_toml(text);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_shader(const PipelineShaderDescriptor& shader)
{
    asset.shader = shader;
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_shader_module(StringView module_path)
{
    asset.shader.module = String(module_path);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_compute_entry(StringView entry)
{
    asset.shader.compute_entry = String(entry);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_macro(StringView key, StringView value)
{
    return with_define(key, value);
}

ComputePipelineBuilder& ComputePipelineBuilder::with_define(StringView key, StringView value)
{
    for (auto& def : asset.shader.defines) {
        if (def.key == key) {
            def.value = String(value);
            return *this;
        }
    }
    asset.shader.defines.push_back(ShaderDefine{String(key), String(value)});
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_defines(View<ShaderDefine> defs)
{
    for (const auto& def : defs) {
        with_define(def.key, def.value);
    }
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_label(CString l)
{
    label = l;
    return *this;
}

ComputePipelineBundle ComputePipelineBuilder::build()
{
    if (error_message) {
        spdlog::error("ComputePipelineBuilder: {}", *error_message);
        return {};
    }

    // 1. synthesize pipeline layout directly from Slang compute reflection
    auto layout = PipelineReflection::reflect_compute_layout(device, compiler, loader, asset.shader);
    if (!layout) {
        spdlog::error("ComputePipelineBuilder: reflection failed for '{}': {}", asset.shader.module, layout.error());
        return {};
    }

    // 2. compile compute shader module once
    CompileDescriptor cdesc{};
    cdesc.path    = asset.shader.module.c_str();
    cdesc.defines = asset.shader.defines;

    auto compiled_module = compiler.compile(cdesc);
    if (!compiled_module) {
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: failed to compile compute shader '{}': {}", asset.shader.module, compiled_module.error());
        return {};
    }

    auto cblob = (*compiled_module)->get_shader_blob(asset.shader.compute_entry.c_str());
    if (!cblob || !cblob->data || cblob->size == 0) {
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: failed to get shader blob for compute entry '{}'", asset.shader.compute_entry);
        return {};
    }

    GPUShaderModuleDescriptor csm_desc{};

    auto clabel    = asset.shader.module + ":" + asset.shader.compute_entry;
    csm_desc.label = clabel.c_str();
    csm_desc.data  = cblob->data;
    csm_desc.size  = cblob->size;
    auto cshader   = device.create_shader_module(csm_desc);
    if (!cshader.valid()) {
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: device failed to create compute shader module: {}", clabel);
        return {};
    }

    // 3. construct GPUComputePipelineDescriptor
    auto pdesc                = asset.to_compute_descriptor();
    pdesc.layout              = layout->layout.handle;
    pdesc.compute.module      = cshader.handle;
    pdesc.compute.entry_point = asset.shader.compute_entry.c_str();
    pdesc.label               = label ? label : asset.shader.module.c_str();

    auto pipeline = device.create_compute_pipeline(pdesc);

    if (!pipeline.valid()) {
        cshader.destroy();
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: device failed to create compute pipeline: {}", pdesc.label ? pdesc.label : "(unlabeled)");
        return {};
    }

    Vector<GPUShaderModule> sms;
    sms.push_back(cshader);

    return ComputePipelineBundle{
        .pipeline       = pipeline,
        .layout         = std::move(*layout),
        .shader_modules = std::move(sms),
    };
}

#pragma endregion ComputePipelineBuilder
