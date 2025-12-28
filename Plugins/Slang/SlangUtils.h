#ifndef LYRA_PLUGIN_SLANG_UTILS_H
#define LYRA_PLUGIN_SLANG_UTILS_H

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

// both Slang and spdlog includes
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Function.h>
#include <Lyra/Plugin/SLC/SLCAPI.h>

using namespace lyra;

using Slang::ComPtr;

auto get_logger() -> Logger;

enum class WalkAction
{
    SKIP,
    CONTINUE,
};

struct CumulativeOffset
{
    int value = 0; // the actual offset
    int space = 0; // the associated space

    friend CumulativeOffset operator+(const CumulativeOffset& lhs, const CumulativeOffset& rhs)
    {
        CumulativeOffset res;
        res.value = lhs.value + rhs.value;
        res.space = lhs.space + rhs.space;
        return res;
    }
};

struct EntryMetadata
{
    GPUShaderStage    stage;
    slang::IMetadata* metadata;
};

struct AccessPathNode
{
    slang::VariableLayoutReflection* layout = nullptr;
    AccessPathNode*                  outer  = nullptr;

    auto calculate_cumulative_offset() const -> CumulativeOffset;
    auto calculate_cumulative_offset(slang::ParameterCategory category) const -> CumulativeOffset;
    bool is_parameter_used(slang::IMetadata* metadata, slang::ParameterCategory unit, CumulativeOffset offset) const;

    void print() const;
};

struct TraversalData
{
    uint current_walk_depth = 0;
};

struct TraverseDepthHandle
{
    TraverseDepthHandle(TraversalData& data) : data(data)
    {
        data.current_walk_depth++;
    }

    ~TraverseDepthHandle()
    {
        data.current_walk_depth--;
    }

    TraversalData& data;
};

struct CompileResultInternal
{
    ComPtr<slang::ISession> session;
    ComPtr<slang::IModule>  module;

    auto get_entry_point(CString entry) const -> ComPtr<slang::IEntryPoint>;
    auto get_linked_program(CString entry) const -> ComPtr<slang::IComponentType>;
    auto get_composed_program(CString entry) const -> ComPtr<slang::IComponentType>;

    bool get_shader_blob(CString entry, ShaderBlob& blob);
};

struct ReflectResultInternal
{
    using Bindings = TreeMap<uint, Vector<GPUBindGroupLayoutEntry>>;
    using Callback = std::function<WalkAction(AccessPathNode)>;

    CompileTarget                target;
    Vector<EntryMetadata>        metadata;
    HashMap<String, uint>        name2attributes;
    HashMap<String, uint>        name2bindgroups;
    HashMap<uint, String>        bind_group_names;
    List<String>                 semantic_names; // just a container to make sure const char* is not lost
    Bindings                     bind_groups;
    Vector<GPUVertexAttribute>   vertex_attributes;
    Vector<GPUPushConstantRange> push_constant_ranges;
    TraversalData                traversal_data;
    uint                         num_push_constant_buffers = 0;
    uint                         msl_parameter_block_space = 0;
    TreeMap<uint, uint>          msl_space_remap;
    bool                         has_error = false;

    bool get_vertex_attributes(ShaderAttributes attrs, GPUVertexAttribute* attributes) const;
    bool get_bind_group_layouts(uint& count, GPUBindGroupLayoutDescriptor* layouts) const;
    bool get_bind_group_location(CString name, uint& group) const;
    bool get_push_constant_ranges(uint& count, GPUPushConstantRange* ranges) const;

    void init(slang::ProgramLayout* program_layout);
    void walk(slang::EntryPointReflection* entry_point, AccessPathNode path, const Callback& callback);
    void walk(slang::VariableLayoutReflection* var_layout, AccessPathNode path, const Callback& callback);

    void init_bindings(slang::ProgramLayout* program_layout);
    void init_vertices(slang::ProgramLayout* program_layout);

    void record_parameter_block_space(AccessPathNode path);
    void create_binding(AccessPathNode path);
    void create_automatic_constant_buffer(AccessPathNode path);
    void create_push_constant(AccessPathNode path, uint space, const GPUBindGroupLayoutEntry& binding);
    void fill_binding_type(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const;
    void fill_binding_index(GPUBindGroupLayoutEntry& entry, CumulativeOffset offset) const;
    void fill_binding_count(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const;
    void fill_binding_stages(GPUBindGroupLayoutEntry& entry, AccessPathNode path) const;
    void fill_dynamic_uniform_buffer(GPUBindGroupLayoutEntry& entry, slang::VariableLayoutReflection* var_layout);
    auto infer_texture_format(slang::TypeLayoutReflection* type) const -> GPUTextureFormat;
    auto infer_vertex_format(slang::TypeLayoutReflection* type) const -> GPUVertexFormat;
    bool is_constant_buffer(AccessPathNode node) const;
};

struct CompilerWrapper
{
    ComPtr<slang::ISession> session;
    ComPtr<slang::IModule>  builtin;

    static void init();

    explicit CompilerWrapper(const CompilerDescriptor& descriptor);

    auto select_profile(const CompilerDescriptor& descriptor) const -> SlangProfileID;

    auto select_target(const CompilerDescriptor& descriptor) const -> SlangCompileTarget;

    void init_builtin_module();

    bool compile(const CompileDescriptor& desc, CompileResultInternal& result);

    bool reflect(ShaderEntryPoints entries, ReflectResultInternal& result);

    CompileTarget target;
};

#endif // LYRA_PLUGIN_SLANG_UTILS_H
