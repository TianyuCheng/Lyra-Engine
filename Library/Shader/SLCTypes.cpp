#include <fstream>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Shader/SLCAPI.h>
#include <Lyra/Shader/SLCTypes.h>

using namespace lyra;

using ShaderPlugin = Plugin<ShaderAPI>;

static Own<ShaderPlugin> SHADER_PLUGIN;

#pragma region Compiler
ShaderAPI* Compiler::api()
{
    return SHADER_PLUGIN->get_api();
}

OwnedResource<Compiler> Compiler::init(const CompilerDescriptor& descriptor)
{
    if (!SHADER_PLUGIN)
        SHADER_PLUGIN = std::make_unique<ShaderPlugin>("lyra-slang");

    OwnedResource<Compiler> compiler(new Compiler());
    Compiler::api()->create_compiler(compiler->handle, descriptor);
    return compiler;
}

void Compiler::destroy()
{
    Compiler::api()->delete_compiler(handle);
}

Own<ShaderModule> Compiler::compile(const Path& path)
{
    std::ifstream file(path);
    assert(file.good());

    // read entire file into buffer
    std::stringstream buffer;
    buffer << file.rdbuf();
    String source = buffer.str();

    auto descriptor   = CompileDescriptor{};
    auto path_u8      = path.u8string();
    auto stem_u8      = path.stem().u8string();
    descriptor.path   = path_u8.c_str();
    descriptor.module = stem_u8.c_str();
    descriptor.source = source.c_str();
    return compile(descriptor);
}

Own<ShaderModule> Compiler::compile(const CompileDescriptor& descriptor)
{
    auto result  = std::make_unique<ShaderModule>();
    bool success = Compiler::api()->create_module(handle, descriptor, result->handle);
    if (!success)
        throw std::runtime_error("Failed to compile shader!");
    return result;
}

Own<ShaderReflection> Compiler::reflect(InitList<ShaderEntryPoint> entry_points)
{
    // make a copy of the entry points
    Vector<ShaderEntryPoint> entries = entry_points;

    auto result  = std::make_unique<ShaderReflection>();
    bool success = Compiler::api()->create_reflection(handle, entries, result->handle);
    if (!success) throw std::runtime_error("Failed to reflect shader!");
    return result;
}
#pragma endregion Compiler

#pragma region ShaderModule
ShaderModule::~ShaderModule()
{
    Compiler::api()->delete_module(handle);
}

OwnedShaderBlob ShaderModule::get_shader_blob(CString entry) const
{
    OwnedShaderBlob  blob;
    ShaderEntryPoint entry_point{handle, entry};
    blob.reset(new ShaderBlob{});
    bool success = Compiler::api()->get_shader_blob(entry_point, *blob.get());
    if (!success) throw std::runtime_error("Failed to reflect shader!");
    return blob;
}
#pragma endregion ShaderModule

#pragma region ShaderReflection
ShaderReflection::~ShaderReflection()
{
    Compiler::api()->delete_reflection(handle);
}

uint ShaderReflection::get_bind_group_location(CString name) const
{
    uint group;
    bool success = Compiler::api()->get_bind_group_location(handle, name, group);
    if (!success) throw std::runtime_error("Failed to reflect bind group location via name!");
    return group;
}

GPUBindGroupLayoutDescriptors ShaderReflection::get_bind_group_layouts()
{
    uint count;
    Compiler::api()->get_bind_group_layouts(handle, count, nullptr);

    bind_group_layouts.resize(count);
    bool success = Compiler::api()->get_bind_group_layouts(handle, count, bind_group_layouts.data());
    if (!success) throw std::runtime_error("Failed to reflect bind group layout descriptors!");
    return bind_group_layouts;
}

GPUPushConstantRanges ShaderReflection::get_push_constant_ranges()
{
    uint count;
    Compiler::api()->get_push_constant_ranges(handle, count, nullptr);

    push_constant_ranges.resize(count);
    bool success = Compiler::api()->get_push_constant_ranges(handle, count, push_constant_ranges.data());
    if (!success) throw std::runtime_error("Failed to reflect push constant ranges!");
    return push_constant_ranges;
}

GPUVertexAttributes ShaderReflection::get_vertex_attributes(ShaderAttributes attrs)
{
    vertex_attributes.push_front({});
    auto& attributes = vertex_attributes.front();
    attributes.resize(attrs.size());

    bool success = Compiler::api()->get_vertex_attributes(handle, attrs, attributes.data());
    if (!success) throw std::runtime_error("Failed to reflect vertex attributes!");
    return attributes;
}

GPUVertexAttributes ShaderReflection::get_vertex_attributes(InitList<ShaderAttribute> attrs)
{
    Vector<ShaderAttribute> copied = attrs;
    return get_vertex_attributes(copied);
}
#pragma endregion ShaderReflection
