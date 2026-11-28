#include <fstream>
#include <sstream>
#include <filesystem>
#include <Lyra/Rendering/PipelineManager.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/Utilities/Logger.h>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;
using namespace lyra;

namespace
{
    // reads text file content from either virtual file loader or local filesystem
    String read_source_text(FileLoader* loader, const String& path_str)
    {
        if (loader && VFSPath::has_protocol(path_str)) {
            auto data = loader->read<char>(path_str.c_str());
            return (!data || data->empty()) ? String{} : String(data->data(), data->size());
        }

        if (!fs::exists(path_str)) {
            return {};
        }

        std::ifstream file(path_str, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    auto compile_shader_module(
        Compiler                       compiler,
        FileLoader*                    loader,
        StringView                     module_path,
        const HashMap<String, String>& macros) -> Result<Own<ShaderModule>>
    {
        auto path_str = String(module_path);
        auto stem_str = fs::path(path_str).stem().string();

        CompileDescriptor desc{};
        desc.path   = path_str.c_str();
        desc.module = stem_str.c_str();

        String source_buffer;
        if (!macros.empty()) {
            String original = read_source_text(loader, path_str);
            if (!original.empty()) {
                for (const auto& [k, v] : macros) {
                    source_buffer += "#define " + k + " " + v + "\n";
                }
                source_buffer += original;
                desc.source = source_buffer.c_str();
            }
        } else if (!VFSPath::has_protocol(path_str)) {
            source_buffer = read_source_text(nullptr, path_str);
            if (!source_buffer.empty()) {
                desc.source = source_buffer.c_str();
            }
        }

        auto module = compiler.compile(desc);
        if (!module) {
            return tl::make_unexpected("PipelineManager: compiler failed for module " + path_str + ": " + module.error());
        }
        return std::move(*module);
    }

    auto create_gpu_shader_module(
        GPUDevice                      device,
        Compiler                       compiler,
        FileLoader*                    loader,
        StringView                     module_path,
        StringView                     entry_point,
        const HashMap<String, String>& macros) -> Result<GPUShaderModule>
    {
        auto compiled_module = compile_shader_module(compiler, loader, module_path, macros);
        if (!compiled_module) {
            return tl::make_unexpected("failed to compile shader module '" + String(module_path) + "': " + compiled_module.error());
        }

        auto blob = (*compiled_module)->get_shader_blob(String(entry_point).c_str());
        if (!blob || !blob->data || blob->size == 0) {
            return tl::make_unexpected("failed to get shader blob for entry: " + String(entry_point));
        }

        GPUShaderModuleDescriptor sm_desc{};
        auto                      label_str = String(module_path) + ":" + String(entry_point);
        sm_desc.label                       = label_str.c_str();
        sm_desc.data                        = blob->data;
        sm_desc.size                        = blob->size;

        auto sm = device.create_shader_module(sm_desc);
        if (!sm.valid()) {
            return tl::make_unexpected("device failed to create shader module: " + label_str);
        }

        return sm;
    }
} // namespace

#pragma region PipelineReflection

auto PipelineReflection::reflect_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc,
    Span<const ShaderEntryPoint>    extra_entries) -> Result<ReflectedPipelineLayout>
{
    auto module = compile_shader_module(compiler, loader, shader_desc.module, shader_desc.macros);
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

auto PipelineReflection::reflect_render_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>
{
    PipelineShaderDescriptor render_shader = shader_desc;
    render_shader.compute_entry.clear();
    return reflect_layout(device, compiler, loader, render_shader);
}

auto PipelineReflection::reflect_compute_layout(
    GPUDevice                       device,
    Compiler                        compiler,
    FileLoader*                     loader,
    const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>
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

RenderPipelineBuilder::RenderPipelineBuilder(PipelineManager& manager)
    : device(manager.get_device()), compiler(manager.get_compiler()), loader(manager.get_file_loader())
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
    auto path_str = String(virtual_path);
    auto text     = read_source_text(loader, path_str);
    if (text.empty()) {
        error_message = "failed to read pipeline asset file: " + path_str;
        return *this;
    }
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
    asset.shader.macros[String(key)] = String(value);
    return *this;
}

RenderPipelineBuilder& RenderPipelineBuilder::with_vertex_layouts(Span<const GPUVertexBufferLayout> layouts)
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

RenderPipelineBuilder& RenderPipelineBuilder::with_targets(Span<const GPUColorTargetState> targets)
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

auto RenderPipelineBuilder::build() -> PipelineBundle
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

    // 2. retrieve or compile shader modules for vertex and fragment stages
    auto vshader = create_gpu_shader_module(device, compiler, loader, asset.shader.module, asset.shader.vertex_entry, asset.shader.macros);
    if (!vshader) {
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: failed vertex shader '{}': {}", asset.shader.vertex_entry, vshader.error());
        return {};
    }

    auto fshader = create_gpu_shader_module(device, compiler, loader, asset.shader.module, asset.shader.fragment_entry, asset.shader.macros);
    if (!fshader) {
        vshader->destroy();
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: failed fragment shader '{}': {}", asset.shader.fragment_entry, fshader.error());
        return {};
    }

    // 3. construct GPURenderPipelineDescriptor
    Vector<GPUColorTargetState> target_storage;
    auto                        pdesc = asset.to_render_descriptor(target_storage);

    pdesc.layout               = layout->layout.handle;
    pdesc.vertex.module        = vshader->handle;
    pdesc.vertex.entry_point   = asset.shader.vertex_entry.c_str();
    pdesc.vertex.buffers       = vertex_layouts;
    pdesc.fragment.module      = fshader->handle;
    pdesc.fragment.entry_point = asset.shader.fragment_entry.c_str();
    pdesc.label                = label ? label : asset.shader.module.c_str();

    auto pipeline = device.create_render_pipeline(pdesc);

    if (!pipeline.valid()) {
        vshader->destroy();
        fshader->destroy();
        layout->destroy();
        spdlog::error("RenderPipelineBuilder: device failed to create render pipeline: {}", pdesc.label ? pdesc.label : "(unlabeled)");
        return {};
    }

    Vector<GPUShaderModule> sms;
    sms.push_back(*vshader);
    sms.push_back(*fshader);

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

ComputePipelineBuilder::ComputePipelineBuilder(PipelineManager& manager)
    : device(manager.get_device()), compiler(manager.get_compiler()), loader(manager.get_file_loader())
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
    auto path_str = String(virtual_path);
    auto text     = read_source_text(loader, path_str);
    if (text.empty()) {
        error_message = "failed to read pipeline asset file: " + path_str;
        return *this;
    }
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
    asset.shader.macros[String(key)] = String(value);
    return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::with_label(CString l)
{
    label = l;
    return *this;
}

auto ComputePipelineBuilder::build() -> ComputePipelineBundle
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

    // 2. retrieve or compile compute shader module
    auto cshader = create_gpu_shader_module(device, compiler, loader, asset.shader.module, asset.shader.compute_entry, asset.shader.macros);
    if (!cshader) {
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: failed compute shader '{}': {}", asset.shader.compute_entry, cshader.error());
        return {};
    }

    // 3. construct GPUComputePipelineDescriptor
    auto pdesc                = asset.to_compute_descriptor();
    pdesc.layout              = layout->layout.handle;
    pdesc.compute.module      = cshader->handle;
    pdesc.compute.entry_point = asset.shader.compute_entry.c_str();
    pdesc.label               = label ? label : asset.shader.module.c_str();

    auto pipeline = device.create_compute_pipeline(pdesc);

    if (!pipeline.valid()) {
        cshader->destroy();
        layout->destroy();
        spdlog::error("ComputePipelineBuilder: device failed to create compute pipeline: {}", pdesc.label ? pdesc.label : "(unlabeled)");
        return {};
    }

    Vector<GPUShaderModule> sms;
    sms.push_back(*cshader);

    return ComputePipelineBundle{
        .pipeline       = pipeline,
        .layout         = std::move(*layout),
        .shader_modules = std::move(sms),
    };
}

#pragma endregion ComputePipelineBuilder

#pragma region PipelineManager

PipelineManager::PipelineManager(GPUDevice device, Compiler compiler, FileLoader* loader)
    : device(device), compiler(compiler), loader(loader)
{
}

PipelineManager::~PipelineManager()
{
    destroy();
}

PipelineManager::PipelineManager(PipelineManager&& other) noexcept
    : device(other.device),
      compiler(other.compiler),
      loader(other.loader),
      cached_layouts(std::move(other.cached_layouts))
{
    other.device   = GPUDevice{};
    other.compiler = Compiler{};
    other.loader   = nullptr;
}

PipelineManager& PipelineManager::operator=(PipelineManager&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    destroy();
    device         = other.device;
    compiler       = other.compiler;
    loader         = other.loader;
    cached_layouts = std::move(other.cached_layouts);

    other.device   = GPUDevice{};
    other.compiler = Compiler{};
    other.loader   = nullptr;
    return *this;
}

bool PipelineManager::init(GPUDevice dev, Compiler comp, FileLoader* file_loader)
{
    destroy();
    device   = dev;
    compiler = comp;
    loader   = file_loader;
    return device.valid();
}

void PipelineManager::destroy()
{
    for (auto& [key, layout] : cached_layouts) {
        layout.destroy();
    }
    cached_layouts.clear();
}

auto PipelineManager::reflect_layout(
    const PipelineShaderDescriptor& shader_desc,
    Span<const ShaderEntryPoint>    extra_entries) -> Result<ReflectedPipelineLayout>
{
    String key = shader_desc.module + ":" + shader_desc.vertex_entry + ":" + shader_desc.fragment_entry + ":" + shader_desc.compute_entry;
    for (const auto& extra : extra_entries) {
        key += "+extra:" + String(extra.entry ? extra.entry : "");
    }
    auto it = cached_layouts.find(key);
    if (it != cached_layouts.end()) {
        return it->second;
    }

    auto layout = PipelineReflection::reflect_layout(device, compiler, loader, shader_desc, extra_entries);
    if (!layout) {
        return layout;
    }
    cached_layouts[key] = *layout;
    return layout;
}

auto PipelineManager::reflect_render_layout(const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>
{
    PipelineShaderDescriptor render_shader = shader_desc;
    render_shader.compute_entry.clear();
    return reflect_layout(render_shader);
}

auto PipelineManager::reflect_compute_layout(const PipelineShaderDescriptor& shader_desc) -> Result<ReflectedPipelineLayout>
{
    PipelineShaderDescriptor compute_shader = shader_desc;
    compute_shader.vertex_entry.clear();
    compute_shader.fragment_entry.clear();
    return reflect_layout(compute_shader);
}

auto PipelineManager::create_render_pipeline(
    const PipelineAsset&              asset,
    Span<const GPUVertexBufferLayout> vertex_layouts,
    CString                           label) -> PipelineBundle
{
    return RenderPipelineBuilder(*this)
        .from_asset(asset)
        .with_vertex_layouts(vertex_layouts)
        .with_label(label)
        .build();
}

auto PipelineManager::create_compute_pipeline(
    const PipelineAsset& asset,
    CString              label) -> ComputePipelineBundle
{
    return ComputePipelineBuilder(*this)
        .from_asset(asset)
        .with_label(label)
        .build();
}

RenderPipelineBuilder PipelineManager::render_pipeline_builder()
{
    return RenderPipelineBuilder(*this);
}

ComputePipelineBuilder PipelineManager::compute_pipeline_builder()
{
    return ComputePipelineBuilder(*this);
}

auto PipelineManager::load_pipeline_asset(StringView virtual_path) const -> Result<PipelineAsset>
{
    auto path_str = String(virtual_path);
    auto text     = read_source_text(loader, path_str);
    if (text.empty()) {
        return tl::make_unexpected("PipelineManager: failed to load pipeline asset file from path: " + path_str);
    }
    return PipelineAsset::deserialize_toml(text);
}

#pragma endregion PipelineManager
