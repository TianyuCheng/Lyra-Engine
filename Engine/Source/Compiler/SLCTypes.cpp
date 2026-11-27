#include <fstream>
#include <Lyra/Utilities/Plugin.h>
#include <Lyra/FileSystem/VFSPath.h>
#include <Lyra/Compiler/SLCAPI.h>
#include <Lyra/Compiler/SLCTypes.h>

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

Result<Own<ShaderModule>> Compiler::compile(const Path& path)
{
    auto path_str     = path.generic_string();
    auto stem_str     = path.stem().string();
    auto descriptor   = CompileDescriptor{};
    descriptor.path   = path_str.c_str();
    descriptor.module = stem_str.c_str();

    String source;
    if (VFSPath::has_protocol(path_str)) {
        descriptor.source = nullptr;
    } else {
        std::ifstream file(path);
        if (!file.good()) {
            return make_unexpected("Failed to open shader file: " + path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        source            = buffer.str();
        descriptor.source = source.c_str();
    }
    return compile(descriptor);
}

Result<Own<ShaderModule>> Compiler::compile(const CompileDescriptor& descriptor)
{
    auto result  = std::make_unique<ShaderModule>();
    bool success = Compiler::api()->create_module(handle, descriptor, result->handle);
    if (!success) {
        return make_unexpected("Failed to compile shader module: " + String(descriptor.module ? descriptor.module : "unknown"));
    }
    return result;
}

Result<Own<ShaderReflection>> Compiler::reflect(ShaderEntryPoints entry_points)
{
    auto result  = std::make_unique<ShaderReflection>();
    bool success = Compiler::api()->create_reflection(handle, entry_points, result->handle);
    if (!success) {
        return make_unexpected("Failed to reflect shader!");
    }
    return result;
}

Result<Own<ShaderReflection>> Compiler::reflect(InitList<ShaderEntryPoint> entry_points)
{
    Vector<ShaderEntryPoint> entries = entry_points;
    return reflect(entries);
}
#pragma endregion Compiler

#pragma region ShaderModule
ShaderModule::~ShaderModule()
{
    Compiler::api()->delete_module(handle);
}

Result<ShaderBlob> ShaderModule::get_shader_blob(CString entry) const
{
    ShaderBlob blob{};
    ShaderEntryPoint entry_point{handle, entry};
    bool success = Compiler::api()->get_shader_blob(entry_point, blob);
    if (!success) {
        return make_unexpected("Failed to get shader blob for entry: " + String(entry ? entry : "null"));
    }
    return blob;
}
#pragma endregion ShaderModule

#pragma region ShaderReflection
ShaderReflection::~ShaderReflection()
{
    Compiler::api()->delete_reflection(handle);
}

Result<uint> ShaderReflection::get_bind_group_location(CString name) const
{
    uint group = 0;
    bool success = Compiler::api()->get_bind_group_location(handle, name, group);
    if (!success) {
        return make_unexpected("Failed to reflect bind group location via name: " + String(name ? name : "null"));
    }
    return group;
}

Result<GPUBindGroupLayoutDescriptors> ShaderReflection::get_bind_group_layouts()
{
    uint count = 0;
    Compiler::api()->get_bind_group_layouts(handle, count, nullptr);

    bind_group_layouts.resize(count);
    bool success = Compiler::api()->get_bind_group_layouts(handle, count, bind_group_layouts.data());
    if (!success) {
        return make_unexpected("Failed to reflect bind group layout descriptors!");
    }
    return bind_group_layouts;
}

Result<uint> ShaderReflection::get_immediate_size()
{
    bool success = Compiler::api()->get_immediate_size(handle, immediate_size);
    if (!success) {
        return make_unexpected("Failed to reflect immediate size!");
    }
    return immediate_size;
}

Result<GPUVertexAttributes> ShaderReflection::get_vertex_attributes(ShaderAttributes attrs)
{
    vertex_attributes.push_front({});
    auto& attributes = vertex_attributes.front();
    attributes.resize(attrs.size());

    bool success = Compiler::api()->get_vertex_attributes(handle, attrs, attributes.data());
    if (!success) {
        return make_unexpected("Failed to reflect vertex attributes!");
    }
    return attributes;
}

Result<GPUVertexAttributes> ShaderReflection::get_vertex_attributes(InitList<ShaderAttribute> attrs)
{
    Vector<ShaderAttribute> copied = attrs;
    return get_vertex_attributes(copied);
}
#pragma endregion ShaderReflection
