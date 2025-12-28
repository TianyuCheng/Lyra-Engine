#ifndef LYRA_PLUGIN_SLANG_UTILS_H
#define LYRA_PLUGIN_SLANG_UTILS_H

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

// both Slang and spdlog includes
#include <sstream>
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

    CumulativeOffset& operator+=(const CumulativeOffset& other)
    {
        this->value += other.value;
        this->space += other.space;
        return *this;
    }
};

struct EntryMetadata
{
    GPUShaderStage    stage;
    slang::IMetadata* metadata;
};

struct AccessPathNode
{
    slang::VariableLayoutReflection* var_layout = nullptr;
    AccessPathNode*                  outer      = nullptr;
};

struct AccessPath
{
    explicit AccessPath() {}

    AccessPathNode* deepest_constant_buffer = nullptr;
    AccessPathNode* deepest_parameter_block = nullptr;
    AccessPathNode* leaf                    = nullptr;

    auto to_string() const -> String
    {
        if (!leaf) return "";

        std::stringstream ss;
        for (auto node = leaf; node != nullptr; node = node->outer) {
            ss << ((node->var_layout->getName()) ? (node->var_layout->getName()) : "unknown");
            ss << "@" << (void*)node->var_layout << " ";
            if (node == deepest_parameter_block) ss << "@paramblock ";
            if (node == deepest_constant_buffer) ss << "@constbuffer ";
            ss << "<- ";
        }
        ss << "root";
        return ss.str();
    }
};

struct ExtendedAccessPath : AccessPath
{
    explicit ExtendedAccessPath(const AccessPath& base, slang::VariableLayoutReflection* var_layout) : AccessPath(base)
    {
        element.var_layout = var_layout;
        element.outer      = leaf;
        leaf               = &element;
    }

    AccessPathNode element;
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
    using Callback = std::function<WalkAction(const AccessPath&)>;

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
    bool                         has_error                 = false;

    bool get_vertex_attributes(ShaderAttributes attrs, GPUVertexAttribute* attributes) const;
    bool get_bind_group_layouts(uint& count, GPUBindGroupLayoutDescriptor* layouts) const;
    bool get_bind_group_location(CString name, uint& group) const;
    bool get_push_constant_ranges(uint& count, GPUPushConstantRange* ranges) const;

    void init(slang::ProgramLayout* program_layout);
    void walk(slang::EntryPointReflection* entry_point, const AccessPath& path, const Callback& callback);
    void walk(slang::VariableLayoutReflection* var_layout, const AccessPath& path, const Callback& callback);

    void init_bindings(slang::ProgramLayout* program_layout);
    void init_vertices(slang::ProgramLayout* program_layout);

    void record_parameter_block_space(const AccessPath& path);
    void create_binding(const AccessPath& path);
    void create_automatic_constant_buffer(const AccessPath& path);
    void create_push_constant(const AccessPath& path, const CumulativeOffset& offset, const GPUBindGroupLayoutEntry& binding);
    void fill_binding_type(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const;
    void fill_binding_index(GPUBindGroupLayoutEntry& entry, CumulativeOffset offset) const;
    void fill_binding_count(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const;
    void fill_binding_stages(GPUBindGroupLayoutEntry& entry, const AccessPath& path) const;
    void fill_dynamic_uniform_buffer(GPUBindGroupLayoutEntry& entry, slang::VariableLayoutReflection* var_layout);
    auto infer_texture_format(slang::TypeLayoutReflection* type) const -> GPUTextureFormat;
    auto infer_vertex_format(slang::TypeLayoutReflection* type) const -> GPUVertexFormat;
    bool is_push_constant_buffer(const AccessPath& node) const;
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
