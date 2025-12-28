// NOTE: This controls whether to dump verbose shader reflection debugging log.
// #define SLANG_DEBUG

#include <iostream>
#include "SlangUtils.h"

static ComPtr<slang::IGlobalSession> GLOBAL_SESSION;

static Logger logger = create_logger("Slang", LogLevel::trace);

static CString builtin_module_source = R"""(
module lyra;

// a custom attribute to indicate if a constant buffer is dynamic uniform buffer
// example usage: [lyra::dynamic]

[__AttributeUsage(_AttributeTargets.Var)]
struct lyra_dynamicAttribute { };
)""";

Logger get_logger()
{
    return logger;
}

static String to_string(GPUShaderStageFlags flags)
{
    String result = "";
    // clang-format off
    if (flags.contains(GPUShaderStage::COMPUTE))  result += "compute ";
    if (flags.contains(GPUShaderStage::VERTEX))   result += "vertex ";
    if (flags.contains(GPUShaderStage::FRAGMENT)) result += "fragment ";
    // clang-format on
    return result;
}

CString to_string(slang::TypeReflection::Kind kind)
{
    // clang-format off
    switch (kind) {
        case slang::TypeReflection::Kind::None:                 return "none";
        case slang::TypeReflection::Kind::Scalar:               return "scalar";
        case slang::TypeReflection::Kind::Vector:               return "vector";
        case slang::TypeReflection::Kind::Matrix:               return "matrix";
        case slang::TypeReflection::Kind::Array:                return "array";
        case slang::TypeReflection::Kind::Struct:               return "struct";
        case slang::TypeReflection::Kind::Resource:             return "resource";
        case slang::TypeReflection::Kind::SamplerState:         return "sampler_state";
        case slang::TypeReflection::Kind::TextureBuffer:        return "texture_buffer";
        case slang::TypeReflection::Kind::ShaderStorageBuffer:  return "shader_storage_buffer";
        case slang::TypeReflection::Kind::ParameterBlock:       return "parameter_block";
        case slang::TypeReflection::Kind::GenericTypeParameter: return "generic_type_parameter";
        case slang::TypeReflection::Kind::Interface:            return "interface";
        case slang::TypeReflection::Kind::OutputStream:         return "output_stream";
        case slang::TypeReflection::Kind::Specialized:          return "specialized";
        case slang::TypeReflection::Kind::Feedback:             return "feedback";
        case slang::TypeReflection::Kind::Pointer:              return "pointer";
        case slang::TypeReflection::Kind::ConstantBuffer:       return "constant_buffer";
        default:                                                return "unknown";
    }
    // clang-format on
}

CString to_string(slang::ParameterCategory category)
{
    // clang-format off
    switch (category) {
        case slang::ParameterCategory::None:                    return "none";
        case slang::ParameterCategory::Mixed:                   return "mixed";
        case slang::ParameterCategory::ConstantBuffer:          return "constant_buffer";
        case slang::ParameterCategory::ShaderResource:          return "shader_resource";
        case slang::ParameterCategory::UnorderedAccess:         return "unordered_access";
        case slang::ParameterCategory::VaryingInput:            return "varying_input";
        case slang::ParameterCategory::VaryingOutput:           return "varying_output";
        case slang::ParameterCategory::SamplerState:            return "sampler_state";
        case slang::ParameterCategory::Uniform:                 return "uniform";
        case slang::ParameterCategory::DescriptorTableSlot:     return "descriptor_table_slot";
        case slang::ParameterCategory::SpecializationConstant:  return "specialization_constant";
        case slang::ParameterCategory::PushConstantBuffer:      return "push_constant_buffer";
        case slang::ParameterCategory::RegisterSpace:           return "register_space";
        case slang::ParameterCategory::GenericResource:         return "generic_resource";
        case slang::ParameterCategory::RayPayload:              return "ray_payload";
        case slang::ParameterCategory::HitAttributes:           return "hit_attributes";
        case slang::ParameterCategory::CallablePayload:         return "callable_payload";
        case slang::ParameterCategory::ShaderRecord:            return "shader_record";
        case slang::ParameterCategory::ExistentialTypeParam:    return "existential_type_param";
        case slang::ParameterCategory::ExistentialObjectParam:  return "existential_object_param";
        case slang::ParameterCategory::MetalPayload:            return "metal_payload";
        case slang::ParameterCategory::MetalAttribute:          return "metal_attribute";
        case slang::ParameterCategory::SubElementRegisterSpace: return "subelement_register_space";
        default:                                                return "unknown";
    }
    // clang-format on
}

template <typename T>
uint round_up_to_next_multiple_of(T size, T align)
{
    return (size + align - 1) / align * align;
}

static void diagnose_if_needed(slang::IBlob* diagnosticsBlob)
{
    if (diagnosticsBlob != nullptr) {
        get_logger()->error("Slang diagnostics: {}", (const char*)diagnosticsBlob->getBufferPointer());
    }
}

static GPUShaderStage to_stage(SlangStage stage)
{
    // clang-format off
    switch (stage) {
        case SLANG_STAGE_VERTEX:         return GPUShaderStage::VERTEX;
        case SLANG_STAGE_FRAGMENT:       return GPUShaderStage::FRAGMENT;
        case SLANG_STAGE_COMPUTE:        return GPUShaderStage::COMPUTE;
        case SLANG_STAGE_RAY_GENERATION: return GPUShaderStage::RAYGEN;
        case SLANG_STAGE_INTERSECTION:   return GPUShaderStage::INTERSECT;
        case SLANG_STAGE_ANY_HIT:        return GPUShaderStage::AHIT;
        case SLANG_STAGE_CLOSEST_HIT:    return GPUShaderStage::CHIT;
        case SLANG_STAGE_MISS:           return GPUShaderStage::MISS;
        case SLANG_STAGE_GEOMETRY:
        case SLANG_STAGE_HULL:
        case SLANG_STAGE_DOMAIN:
        case SLANG_STAGE_AMPLIFICATION:
        case SLANG_STAGE_MESH:
        case SLANG_STAGE_CALLABLE:
        default:
            assert(!!!"Unsupported shader types!");
            return GPUShaderStage::COMPUTE;
    }
    // clang-format on
}

static CumulativeOffset calculate_cumulative_offset(slang::ParameterCategory layout_unit, AccessPath access_path)
{
    CumulativeOffset result;
    switch (layout_unit) {
        // layout units that care about spaces
        case slang::ParameterCategory::SamplerState:
        case slang::ParameterCategory::ConstantBuffer:
        case slang::ParameterCategory::ShaderResource:
        case slang::ParameterCategory::UnorderedAccess:
        case slang::ParameterCategory::DescriptorTableSlot:
            for (auto node = access_path.leaf; node != access_path.deepest_parameter_block; node = node->outer) {
                result.value += static_cast<int>(node->var_layout->getOffset(layout_unit));
                result.space += static_cast<int>(node->var_layout->getBindingSpace(layout_unit));
            }
            for (auto node = access_path.deepest_parameter_block; node != nullptr; node = node->outer) {
                result.space += static_cast<int>(node->var_layout->getOffset(
                    slang::ParameterCategory::SubElementRegisterSpace));
            }
            break;

        // bytes
        case slang::ParameterCategory::Uniform:
            for (auto node = access_path.leaf; node != access_path.deepest_constant_buffer; node = node->outer)
                result.value += static_cast<int>(node->var_layout->getOffset(layout_unit));
            break;

        // layout units that don't require special handling
        default:
            for (auto node = access_path.leaf; node != nullptr; node = node->outer)
                result.value += static_cast<int>(node->var_layout->getOffset(layout_unit));
            break;
    }
    return result;
}

static uint calculate_cumulative_space(CompileTarget target, const AccessPath& path)
{
    uint space = 0;
    switch (target) {
        case CompileTarget::MSL:
            space += calculate_cumulative_offset(slang::ParameterCategory::ConstantBuffer, path).space;
            space += calculate_cumulative_offset(slang::ParameterCategory::SubElementRegisterSpace, path).space;
            break;
        case CompileTarget::DXIL:
            space += calculate_cumulative_offset(slang::ParameterCategory::ConstantBuffer, path).space;
            space += calculate_cumulative_offset(slang::ParameterCategory::DescriptorTableSlot, path).space;
            space += calculate_cumulative_offset(slang::ParameterCategory::SubElementRegisterSpace, path).space;
            break;
        case CompileTarget::SPIRV:
            space += calculate_cumulative_offset(slang::ParameterCategory::DescriptorTableSlot, path).space;
            space += calculate_cumulative_offset(slang::ParameterCategory::SubElementRegisterSpace, path).space;
            break;
    }
    return space;
}

static GPUShaderStageFlags calculate_shader_stage_mask(const Vector<EntryMetadata>& metadata, AccessPath path)
{
    GPUShaderStageFlags result;

    auto node = path.leaf;
    if (!node) return GPUShaderStageFlags();

    for (uint i = 0; i < node->var_layout->getCategoryCount(); i++) {
        auto unit   = node->var_layout->getCategoryByIndex(i);
        auto offset = calculate_cumulative_offset(unit, path);
        for (auto meta : metadata) {
            bool used = false;
            meta.metadata->isParameterLocationUsed(
                SlangParameterCategory(unit),
                offset.space,
                offset.value,
                used);
            if (used) result.set(meta.stage);
        }
    }
    return result;
}

static uint get_shader_entry_point_index(slang::ProgramLayout* layout, slang::EntryPointReflection* refl)
{
    for (uint i = 0; i < layout->getEntryPointCount(); i++)
        if (layout->getEntryPointByIndex(i) == refl)
            return i;

    assert(!!!"Failed to find shader entry point!");
    return ~0u;
}

static void print_slang_var_layout(const Vector<EntryMetadata>& metadata, const AccessPath& curr, slang::VariableLayoutReflection* var_layout, TraversalData& traversal)
{
    int  walk_depth  = traversal.current_walk_depth;
    auto print_depth = [&](CString prefix = "  ") -> std::ostream& {
        for (int i = 0; i < walk_depth; i++)
            std::cout << prefix;
        return std::cout;
    };
    print_depth() << "# PATH: " << curr.to_string() << std::endl;
    if (var_layout->getName())
        print_depth() << "# NAME: " << var_layout->getName() << std::endl;
    else
        print_depth() << "# NAME: unknwon" << std::endl;

    auto path       = ExtendedAccessPath(curr, var_layout);
    auto typ_layout = var_layout->getTypeLayout();
    print_depth() << "= type layout: " << to_string(typ_layout->getKind()) << std::endl;
    print_depth() << "= type unit: " << to_string(typ_layout->getParameterCategory());
    std::cout << " @space: " << var_layout->getBindingSpace((SlangParameterCategory)typ_layout->getParameterCategory());
    std::cout << " @offset: " << var_layout->getOffset((SlangParameterCategory)typ_layout->getParameterCategory()) << std::endl;

    // offset from each category
    for (uint i = 0; i < var_layout->getCategoryCount(); i++) {
        auto unit       = var_layout->getCategoryByIndex(i);
        auto cumulative = calculate_cumulative_offset(unit, path);
        print_depth() << "= var unit: " << to_string(unit);
        std::cout << " @space: " << cumulative.space;
        std::cout << " @offset: " << cumulative.value << std::endl;
    }

    // visibility
    auto visibility = calculate_shader_stage_mask(metadata, path);
    print_depth() << "= visibility: " << to_string(visibility) << std::endl;
    std::cout << std::endl;
}

#pragma region CompilerWrapper
void CompilerWrapper::init()
{
    createGlobalSession(GLOBAL_SESSION.writeRef());
}

CompilerWrapper::CompilerWrapper(const CompilerDescriptor& descriptor)
{
    this->target = descriptor.target;

    Vector<slang::CompilerOptionEntry> options;

    static String root_constant_key = "PUSH_CONSTANT";
    static String root_constant_val = "register(b0, space" + std::to_string(D3D12_PushConstantRegisterSpace) + ")";
    static String root_constant_msl = "register(b" + std::to_string(METAL_PushConstantBufferIndex) + ")";

    // special treatment for root constants
    {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::MacroDefine;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = root_constant_key.c_str();
        entry.value.stringValue1 = target == CompileTarget::MSL
                                       ? root_constant_msl.c_str()
                                       : root_constant_val.c_str();
        options.push_back(entry);
    }

    // invert y-axis for spirv-based shaders
    if (target == CompileTarget::SPIRV) {
        auto entry            = slang::CompilerOptionEntry{};
        entry.name            = slang::CompilerOptionName::VulkanInvertY;
        entry.value.intValue0 = 1;
        options.push_back(entry);
    }

    // include dirs
    for (auto& include : descriptor.includes) {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::Include;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = include;
        options.push_back(entry);
    }

    // preprocessor macros
    for (auto& macro : descriptor.defines) {
        auto entry               = slang::CompilerOptionEntry{};
        entry.name               = slang::CompilerOptionName::MacroDefine;
        entry.value.kind         = slang::CompilerOptionValueKind::String;
        entry.value.stringValue0 = macro.key;
        entry.value.stringValue1 = macro.value;
        options.push_back(entry);
    }

    // emit option (SPIRV)
    if (target == CompileTarget::SPIRV) {
        auto emit            = slang::CompilerOptionEntry{};
        emit.name            = slang::CompilerOptionName::EmitSpirvDirectly;
        emit.value.kind      = slang::CompilerOptionValueKind::Int;
        emit.value.intValue0 = 1;
        options.push_back(emit);
    }

    // preserve entry name (SPIRV)
    if (target == CompileTarget::SPIRV) {
        auto emit            = slang::CompilerOptionEntry{};
        emit.name            = slang::CompilerOptionName::VulkanUseEntryPointName;
        emit.value.kind      = slang::CompilerOptionValueKind::Int;
        emit.value.intValue0 = 1;
        options.push_back(emit);
    }

    auto target_desc    = slang::TargetDesc{};
    target_desc.format  = select_target(descriptor);
    target_desc.profile = select_profile(descriptor);

    auto session_desc                     = slang::SessionDesc{};
    session_desc.targets                  = &target_desc;
    session_desc.targetCount              = 1;
    session_desc.compilerOptionEntryCount = static_cast<uint>(options.size());
    session_desc.compilerOptionEntries    = options.data();

    GLOBAL_SESSION->createSession(session_desc, session.writeRef());

    // some common stuff that might be used in other shaders
    init_builtin_module();
}

SlangProfileID CompilerWrapper::select_profile(const CompilerDescriptor& descriptor) const
{
    switch (descriptor.target) {
        case CompileTarget::MSL:
            return GLOBAL_SESSION->findProfile("msl_2_4");
        case CompileTarget::DXIL:
            return GLOBAL_SESSION->findProfile("sm_6_5");
        case CompileTarget::SPIRV:
        default: // fallback for invalid arguments
            return GLOBAL_SESSION->findProfile("spirv_1_5");
    }
}

SlangCompileTarget CompilerWrapper::select_target(const CompilerDescriptor& descriptor) const
{
    switch (descriptor.target) {
        case CompileTarget::MSL:
            return SLANG_METAL_LIB;
        case CompileTarget::DXIL:
            return SLANG_DXIL;
        case CompileTarget::SPIRV:
        default:
            return SLANG_SPIRV;
    }
}

void CompilerWrapper::init_builtin_module()
{
    // create shader module
    ComPtr<slang::IBlob> diagnostics;
    builtin = session->loadModuleFromSourceString(
        "lyra",                  // module name
        "lyra.slang",            // module path
        builtin_module_source,   // shader source code
        diagnostics.writeRef()); // optional diagnostic container
    diagnose_if_needed(diagnostics);
}

bool CompilerWrapper::compile(const CompileDescriptor& desc, CompileResultInternal& result)
{
    // compile result shares the session
    result.session = session;

    // create shader module
    ComPtr<slang::IBlob> diagnostics;
    result.module = session->loadModuleFromSourceString(
        desc.module,             // module name
        desc.path,               // module path
        desc.source,             // shader source code
        diagnostics.writeRef()); // optional diagnostic container
    diagnose_if_needed(diagnostics);

    return result.module != nullptr;
}

bool CompilerWrapper::reflect(ShaderEntryPoints entries, ReflectResultInternal& result)
{
    // make sure single module is added exactly once
    HashSet<slang::IComponentType*> modules;
    for (auto& entry : entries) {
        auto module = reinterpret_cast<CompileResultInternal*>(entry.module.pointer);
        modules.insert(module->module);
    }

    // find all required shader entry points
    Vector<slang::IComponentType*>     component_types(modules.begin(), modules.end());
    Vector<ComPtr<slang::IEntryPoint>> entry_points;
    for (auto& entry : entries) {
        auto module = reinterpret_cast<CompileResultInternal*>(entry.module.pointer);
        auto entryp = module->get_entry_point(entry.entry);
        entry_points.push_back(entryp);
        component_types.push_back(entryp);
    }

    // create a composed program for reflection
    ComPtr<slang::IComponentType> composed_program;
    {
        ComPtr<slang::IBlob> diagnostics;
        SlangResult          result = session->createCompositeComponentType(
            component_types.data(),
            component_types.size(),
            composed_program.writeRef(),
            diagnostics.writeRef());
        diagnose_if_needed(diagnostics);
        SLANG_RETURN_FALSE_ON_FAIL(result);
    }

    // link the program
    ComPtr<slang::IComponentType> linked_program;
    {
        ComPtr<slang::IBlob> diagnostics;
        auto                 res = composed_program->link(
            linked_program.writeRef(),
            diagnostics.writeRef());
        diagnose_if_needed(diagnostics);
        SLANG_RETURN_FALSE_ON_FAIL(res);
    }

    // associate stages with ICompileRequests*
    auto layout = linked_program->getLayout();
    for (auto& entry : entries) {
        auto refl  = layout->findEntryPointByName(entry.entry);
        uint index = get_shader_entry_point_index(layout, refl);
        auto stage = to_stage(refl->getStage());

        slang::IMetadata*    metadata;
        ComPtr<slang::IBlob> diagnostics;

        auto res = linked_program->getEntryPointMetadata(index, 0, &metadata, diagnostics.writeRef());
        diagnose_if_needed(diagnostics);
        SLANG_RETURN_FALSE_ON_FAIL(res);

        result.metadata.push_back(EntryMetadata{stage, metadata});
    }

    result.target = target;
    result.init(layout);
    return !result.has_error;
}
#pragma endregion CompilerWrapper

#pragma region CompileResultInternal
ComPtr<slang::IEntryPoint> CompileResultInternal::get_entry_point(CString entry) const
{
    // query entry point
    ComPtr<slang::IEntryPoint> entry_point;
    ComPtr<slang::IBlob>       diagnostics;
    module->findEntryPointByName(entry, entry_point.writeRef());

    if (!entry_point) {
        get_logger()->error("Failed to get entry point: {}", entry);
        return nullptr;
    }
    return entry_point;
}

ComPtr<slang::IComponentType> CompileResultInternal::get_linked_program(CString entry) const
{
    auto composed_program = get_composed_program(entry);
    if (composed_program == nullptr)
        return nullptr;

    // link
    ComPtr<slang::IComponentType> linked_program;
    ComPtr<slang::IBlob>          diagnostics;

    SlangResult result = composed_program->link(
        linked_program.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics);

    if (SLANG_FAILED(result)) {
        get_logger()->error("Failed to link program: {}", entry);
        return nullptr;
    }
    return linked_program;
}

ComPtr<slang::IComponentType> CompileResultInternal::get_composed_program(CString entry) const
{
    auto entry_point = get_entry_point(entry);
    if (entry_point == nullptr)
        return nullptr;

    // compose modules + entry points
    std::array<slang::IComponentType*, 2> component_types = {module, entry_point};
    ComPtr<slang::IComponentType>         composed_program;
    ComPtr<slang::IBlob>                  diagnostics;

    SlangResult result = session->createCompositeComponentType(
        component_types.data(),
        component_types.size(),
        composed_program.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics);

    if (SLANG_FAILED(result)) {
        get_logger()->error("Failed to compose program: {}", entry);
        return nullptr;
    }
    return composed_program;
}

bool CompileResultInternal::get_shader_blob(CString entry, ShaderBlob& blob)
{
    auto linked_program = get_linked_program(entry);
    if (linked_program == nullptr)
        return false;

    // kernel binary
    ComPtr<slang::IBlob> spirv_code;
    ComPtr<slang::IBlob> diagnostics;

    SlangResult result;
    result = linked_program->getEntryPointCode(
        0,
        0,
        spirv_code.writeRef(),
        diagnostics.writeRef());
    diagnose_if_needed(diagnostics);
    SLANG_RETURN_ON_FAIL(result);

    blob.size = static_cast<uint32_t>(spirv_code->getBufferSize());
    blob.data = new uint8_t[blob.size];
    std::memcpy((uint8_t*)blob.data, (uint8_t*)spirv_code->getBufferPointer(), blob.size);
    return true;
}
#pragma endregion CompileResultInternal

#pragma region ReflectResultInternal
bool ReflectResultInternal::get_vertex_attributes(ShaderAttributes attrs, GPUVertexAttribute* attributes) const
{
    if (has_error) return false;

    // assume attributes has been properly allocated
    bool pass = true;
    for (uint i = 0; i < attrs.size(); i++) {
        auto& attr = attrs.at(i);
        auto  iter = name2attributes.find(attr.name);
        if (iter == name2attributes.end()) {
            pass = false;
            get_logger()->error("Failed to find vertex attribute with name: {}!", attr.name);
        } else {
            attributes[i]        = vertex_attributes.at(iter->second);
            attributes[i].offset = attr.offset;
        }
    }
    return pass;
}

bool ReflectResultInternal::get_bind_group_layouts(uint& count, GPUBindGroupLayoutDescriptor* layouts) const
{
    if (has_error) return false;

    count = static_cast<uint>(bind_groups.size());

    // check if layouts is provided, if null, simply return count
    if (layouts == nullptr)
        return true;

    // map Slang spaces to continuous indices
    TreeMap<uint, uint> space2index;
    uint                current_index = 0;
    for (const auto& [space, entries] : bind_groups) {
        space2index[space] = current_index++;
    }

    // initialize layouts (assume layout has been properly allocated)
    for (const auto& [space, entries] : bind_groups) {
        auto layout                 = GPUBindGroupLayoutDescriptor{};
        auto it                     = bind_group_names.find(space);
        layout.label                = (it != bind_group_names.end()) ? it->second.c_str() : nullptr;
        layout.entries              = entries;
        layouts[space2index[space]] = layout;
    }
    return true;
}

bool ReflectResultInternal::get_bind_group_location(CString name, uint& group) const
{
    if (has_error) return false;

    auto it = name2bindgroups.find(name);
    if (it == name2bindgroups.end()) {
        get_logger()->error("Failed to find bind group with name: {}!", name);
        return false;
    }

    // assign the group and binding
    group = it->second;
    return true;
}

bool ReflectResultInternal::get_push_constant_ranges(uint& count, GPUPushConstantRange* ranges) const
{
    if (has_error) return false;

    count = static_cast<uint>(push_constant_ranges.size());

    // check if ranges is provided, if null, simply return count
    if (ranges == nullptr)
        return true;

    // copy the push constant ranges to caller
    for (uint i = 0; i < count; i++)
        ranges[i] = push_constant_ranges.at(i);

    return true;
}

void ReflectResultInternal::init(slang::ProgramLayout* program_layout)
{
    init_bindings(program_layout);
    init_vertices(program_layout);
}

void ReflectResultInternal::walk(slang::EntryPointReflection* entry_point, const AccessPath& path, const Callback& callback)
{
    auto var_layout = entry_point->getVarLayout();
    auto path_node  = ExtendedAccessPath(path, var_layout);
    walk(var_layout, path_node, callback);
}

void ReflectResultInternal::walk(slang::VariableLayoutReflection* var_layout, const AccessPath& path, const Callback& callback)
{
#ifdef SLANG_DEBUG
    print_slang_var_layout(metadata, path, var_layout, traversal_data);

    // a handle that automatically increment/decrement the traversal depth
    TraverseDepthHandle depth_walker(traversal_data);
#endif

    auto typ_layout = var_layout->getTypeLayout();
    auto path_node  = ExtendedAccessPath(path, var_layout);

    // record deepest constant buffer / parameter block
    switch (typ_layout->getKind()) {
        case slang::TypeReflection::Kind::TextureBuffer:
        case slang::TypeReflection::Kind::ConstantBuffer:
        case slang::TypeReflection::Kind::ParameterBlock:
        case slang::TypeReflection::Kind::ShaderStorageBuffer:
        {
            auto container_var_layout = typ_layout->getContainerVarLayout();

            path_node.deepest_constant_buffer = path_node.leaf;
            if (container_var_layout->getTypeLayout()->getSize(slang::ParameterCategory::SubElementRegisterSpace) != 0)
                path_node.deepest_parameter_block = path_node.leaf;

            break;
        }
        default:
            break;
    }

    // invoke callback
    if (callback(path_node) == WalkAction::SKIP)
        return;

    // special case for parameter block
    if (typ_layout->getKind() == slang::TypeReflection::Kind::ParameterBlock) {
        auto inner = typ_layout->getElementVarLayout();
        return walk(inner, path_node, callback);
    }

    // traverse
    uint field_count = typ_layout->getFieldCount();
    for (uint i = 0; i < field_count; i++) {
        auto field_var = typ_layout->getFieldByIndex(i);
        walk(field_var, path_node, callback);
    }
}

void ReflectResultInternal::init_bindings(slang::ProgramLayout* program_layout)
{
    auto callback = [&](AccessPath path) {
        auto typ_layout = path.leaf->var_layout->getTypeLayout();
        switch (typ_layout->getKind()) {
            case slang::TypeReflection::Kind::ParameterBlock:
                // ParameterBlock has a name, we record it for bind group retrieval
                record_parameter_block_space(path);

                // ParameterBlock will automatically introduce a constant buffer binding if ordinary types are observed.
                if (typ_layout->getElementTypeLayout()->getSize())
                    create_automatic_constant_buffer(path);

                return WalkAction::CONTINUE;
            case slang::TypeReflection::Kind::Resource:
            case slang::TypeReflection::Kind::SamplerState:
            case slang::TypeReflection::Kind::TextureBuffer:
            case slang::TypeReflection::Kind::ConstantBuffer:
            case slang::TypeReflection::Kind::ShaderStorageBuffer:
                create_binding(path);
                return WalkAction::SKIP;
            default:
                return WalkAction::CONTINUE;
        }
    };

    // reset traversal data
    traversal_data.current_walk_depth = 0;

    // iterate through global parameters
    auto global_params = program_layout->getGlobalParamsVarLayout();
    walk(global_params, AccessPath{}, callback);

    // iterate through entry points
    auto entry_point_count = program_layout->getEntryPointCount();
    for (uint i = 0; i < entry_point_count; i++) {
        auto entry_point_reflect = program_layout->getEntryPointByIndex(i);
        walk(entry_point_reflect, AccessPath{}, callback);
    }

    // organize the bindings
}

void ReflectResultInternal::init_vertices(slang::ProgramLayout* program_layout)
{
    auto callback = [&](AccessPath path) {
        auto node       = path.leaf;
        auto typ_layout = node->var_layout->getTypeLayout();
        if (typ_layout->getParameterCategory() == slang::ParameterCategory::VertexInput) {
            if (typ_layout->getKind() != slang::TypeReflection::Kind::Struct) {
                auto name           = node->var_layout->getName();
                auto semantic_name  = node->var_layout->getSemanticName();
                uint semantic_index = static_cast<uint>(node->var_layout->getSemanticIndex());
                uint binding_index  = static_cast<uint>(node->var_layout->getBindingIndex());
                get_logger()->trace("[VTX INPUT] NAME: {}\t LOCATION: {} SEMANTICS: {}{}", name, binding_index, semantic_name, semantic_index);

                semantic_names.push_front(semantic_name);

                vertex_attributes.push_back(GPUVertexAttribute{});
                auto& attribute           = vertex_attributes.back();
                attribute.offset          = 0; // host-provided, cannot be reflected from shader
                attribute.format          = infer_vertex_format(typ_layout);
                attribute.shader_semantic = semantic_names.front().c_str();
                attribute.shader_location = target == CompileTarget::DXIL ? semantic_index : binding_index;

                name2attributes.emplace(name, static_cast<uint>(vertex_attributes.size() - 1));
            }
        }
        return WalkAction::CONTINUE;
    };

    // iterate through entry points (only process vertex shader entry)
    uint entry_point_count = static_cast<uint>(program_layout->getEntryPointCount());
    for (uint i = 0; i < entry_point_count; i++) {
        auto entry_point_reflect = program_layout->getEntryPointByIndex(i);
        if (entry_point_reflect->getStage() == SLANG_STAGE_VERTEX)
            walk(entry_point_reflect, AccessPath{}, callback);
    }
}

void ReflectResultInternal::record_parameter_block_space(const AccessPath& path)
{
    auto node = path.leaf;
    assert(node->var_layout->getTypeLayout()->getKind() == slang::TypeReflection::Kind::ParameterBlock);

    auto name  = node->var_layout->getName();
    uint space = calculate_cumulative_space(target, path);

    get_logger()->trace("Recording parameter block: {} with space: {}", name, space);
    name2bindgroups.emplace(name, space);
    bind_group_names.emplace(space, name);
}

void ReflectResultInternal::create_automatic_constant_buffer(const AccessPath& path)
{
    auto node    = path.leaf;
    auto offset  = calculate_cumulative_offset(slang::ParameterCategory::ConstantBuffer, path);
    offset.space = calculate_cumulative_space(target, path);

    auto val  = GPUBindGroupLayoutEntry{};
    val.type  = GPUResourceType::BUFFER;
    val.count = 1;
    fill_binding_index(val, offset);
    fill_binding_stages(val, path);
    fill_dynamic_uniform_buffer(val, node->var_layout);

    if (is_push_constant_buffer(path)) {
        create_push_constant(path, offset.space, val);
    } else {
        bind_groups[offset.space].push_back(val);
        get_logger()->trace("[BINDGROUP] NAME:{}\t SPACE:{} BINDING:{} (AUTOMATIC)", node->var_layout->getName(), offset.space, offset.value);
    }
}

void ReflectResultInternal::create_binding(const AccessPath& path)
{
    auto node    = path.leaf;
    auto type    = node->var_layout->getTypeLayout();
    auto offset  = calculate_cumulative_offset(type->getParameterCategory(), path);
    offset.space = calculate_cumulative_space(target, path);

    auto val = GPUBindGroupLayoutEntry{};
    fill_binding_type(val, type);
    fill_binding_index(val, offset);
    fill_binding_count(val, type);
    fill_binding_stages(val, path);
    fill_dynamic_uniform_buffer(val, node->var_layout);

    // check for push constant vs constant buffer view binding
    if (is_push_constant_buffer(path)) {
        create_push_constant(path, offset.space, val);
    } else {
        // append to bindings
        bind_groups[offset.space].push_back(val);
        get_logger()->trace("[BINDGROUP] NAME:{}\t SPACE:{} BINDING:{}", node->var_layout->getName(), offset.space, offset.value);
    }
}

void ReflectResultInternal::create_push_constant(const AccessPath& path, uint space, const GPUBindGroupLayoutEntry& binding)
{
    auto node = path.leaf;

    // enforce that we must use PUSH_CONSTANT macro to annotate the push constant constant buffer.
    uint push_constant_slot = target == CompileTarget::MSL
                                  ? METAL_PushConstantBufferIndex
                                  : D3D12_PushConstantRegisterSpace;

    if (space != push_constant_slot) {
        get_logger()->error("Please use ROOT_CONSTANT to annotate the binding register space, found {}, expected {}", space, push_constant_slot);
        has_error = true;
        return;
    }

    if (node->var_layout->getType()->getKind() != slang::TypeReflection::Kind::ConstantBuffer) {
        get_logger()->error("Please directly define push constant / root constant using ConstantBuffer<T>.");
        has_error = true;
        return;
    }

    if (num_push_constant_buffers++ >= 1) {
        get_logger()->error("Please specify all push constant / root constant using only one ConstantBuffer<T>. Using multiple ConstantBuffer<T> is not allowed!");
        has_error = true;
        return;
    }

    // reflect each field in the push constant block
    auto push_constant_type = node->var_layout->getTypeLayout()->getElementTypeLayout();
    for (unsigned j = 0; j < push_constant_type->getFieldCount(); j++) {
        auto push_constant_field  = push_constant_type->getFieldByIndex(j);
        auto push_constant_size   = push_constant_field->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM);
        auto push_constant_offset = push_constant_field->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM);
        auto push_constant_range  = GPUPushConstantRange{
            static_cast<uint>(push_constant_offset),
            static_cast<uint>(push_constant_size),
            binding.visibility, // TODO: This is a hack for now. We are populating the visibility of push constants at block level. This is not correct.
        };
        get_logger()->trace("[PUSH CONSTANT] NAME:{}.{}\t OFFSET:{} SIZE:{}",
            node->var_layout->getName(),
            push_constant_field->getName(),
            push_constant_range.offset,
            push_constant_range.size);
        push_constant_ranges.push_back(push_constant_range);
    }

    // fallback for basic types (non-struct types)
    if (push_constant_type->getFieldCount() == 0) {
        auto push_constant_range = GPUPushConstantRange{
            static_cast<uint>(0),
            static_cast<uint>(push_constant_type->getSize()),
            binding.visibility,
        };
        get_logger()->trace("[PUSH CONSTANT] NAME:{}\t OFFSET:{} SIZE:{}",
            node->var_layout->getName(),
            push_constant_range.offset,
            push_constant_range.size);
        push_constant_ranges.push_back(push_constant_range);
    }
}

void ReflectResultInternal::fill_binding_index(GPUBindGroupLayoutEntry& entry, CumulativeOffset offset) const
{
    uint existing_binding_count = 0;

    auto it = bind_groups.find((uint)offset.space);
    if (it != bind_groups.end())
        existing_binding_count = static_cast<uint>(it->second.size());

    if (target != CompileTarget::DXIL) {
        entry.binding.index          = offset.value;
        entry.binding.register_index = 0; // only DXIL has register space
        return;
    } else {
        entry.binding.index          = existing_binding_count;
        entry.binding.register_index = offset.value;
    }
}

void ReflectResultInternal::fill_binding_count(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const
{
    // entry.bindless = false;

    if (!type->isArray()) {
        entry.count = 1;
        return;
    }

    // possibly unbound (when count = ~size_t(0))
    size_t count = type->getTotalArrayElementCount();
    entry.count  = static_cast<uint>(count);
}

void ReflectResultInternal::fill_binding_stages(GPUBindGroupLayoutEntry& entry, const AccessPath& path) const
{
    // for Metal shading language, the shader stage visibility is implicit from the shader entry arguments.
    // MSL has a different binding model than SPIRV/DXIL, it uses argument buffer instead of descriptor table slot,
    // or subelement register space. The visibility is operated at the argument buffer level.
    if (target == CompileTarget::MSL)
        return;

    // implement this using IMetadata (or ICompileRequests for older versions of Slang)
    entry.visibility = calculate_shader_stage_mask(metadata, path);
}

void ReflectResultInternal::fill_binding_type(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const
{
    auto kind = type->getKind();

    // common stuff
    auto view_dimension    = GPUTextureViewDimension::x1D;
    auto access_permission = GPUStorageTextureAccess::READ_ONLY;

    switch (kind) {
        case slang::TypeReflection::Kind::Resource:
        {
            auto shape  = type->getResourceShape();
            auto access = type->getResourceAccess();

            // view dimension
            // clang-format off
            switch (shape & SLANG_RESOURCE_BASE_SHAPE_MASK) {
                case SLANG_TEXTURE_1D:         view_dimension = GPUTextureViewDimension::x1D;        break;
                case SLANG_TEXTURE_2D:         view_dimension = GPUTextureViewDimension::x2D;        break;
                case SLANG_TEXTURE_2D_ARRAY:   view_dimension = GPUTextureViewDimension::x2D_ARRAY;  break;
                case SLANG_TEXTURE_3D:         view_dimension = GPUTextureViewDimension::x3D;        break;
                case SLANG_TEXTURE_CUBE:       view_dimension = GPUTextureViewDimension::CUBE;       break;
                case SLANG_TEXTURE_CUBE_ARRAY: view_dimension = GPUTextureViewDimension::CUBE_ARRAY; break;
                case SLANG_TEXTURE_1D_ARRAY:   assert(!!!"TEXTURE1D_ARRAY is not supported!");       break;
                default:
                    break;
            }
            // clang-format on

            // texture access
            // clang-format off
            switch (access) {
                case SLANG_RESOURCE_ACCESS_READ:       access_permission = GPUStorageTextureAccess::READ_ONLY;  break;
                case SLANG_RESOURCE_ACCESS_WRITE:      access_permission = GPUStorageTextureAccess::WRITE_ONLY; break;
                case SLANG_RESOURCE_ACCESS_READ_WRITE: access_permission = GPUStorageTextureAccess::READ_WRITE; break;
                default:                                                                                        break;
            }
            // clang-format on

            switch (shape & SLANG_RESOURCE_BASE_SHAPE_MASK) {
                case SLANG_TEXTURE_1D:
                case SLANG_TEXTURE_2D:
                case SLANG_TEXTURE_3D:
                case SLANG_TEXTURE_CUBE:
                    entry.type = (access_permission != GPUStorageTextureAccess::READ_ONLY)
                                     ? GPUResourceType::STORAGE_TEXTURE
                                     : GPUResourceType::TEXTURE;
                    if (entry.type == GPUResourceType::STORAGE_TEXTURE) {
                        entry.storage_texture.view_dimension = view_dimension;
                        entry.storage_texture.access         = access_permission;
                        entry.storage_texture.format         = infer_texture_format(type);
                    } else {
                        entry.texture.view_dimension = view_dimension;
                        entry.texture.multisampled   = shape & SLANG_TEXTURE_MULTISAMPLE_FLAG;
                        entry.texture.sample_type    = GPUTextureSampleType::FLOAT;
                    }
                    return;
                case SLANG_STRUCTURED_BUFFER:
                case SLANG_BYTE_ADDRESS_BUFFER:
                    entry.type                      = GPUResourceType::BUFFER;
                    entry.buffer.type               = (access_permission != GPUStorageTextureAccess::READ_ONLY)
                                                          ? GPUBufferBindingType::READ_ONLY_STORAGE
                                                          : GPUBufferBindingType::STORAGE;
                    entry.buffer.has_dynamic_offset = false;
                    entry.buffer.min_binding_size   = type->getSize();
                    return;
                case SLANG_ACCELERATION_STRUCTURE:
                    entry.type              = GPUResourceType::ACCELERATION_STRUCTURE;
                    entry.bvh.vertex_return = false;
                    return;
                default:
                    entry.type = GPUResourceType::TEXTURE;
                    return;
            }
        }

        case slang::TypeReflection::Kind::SamplerState:
            entry.type         = GPUResourceType::SAMPLER;
            entry.sampler.type = GPUSamplerBindingType::FILTERING;
            return;

        case slang::TypeReflection::Kind::TextureBuffer:
        case slang::TypeReflection::Kind::ShaderStorageBuffer:
            entry.type        = GPUResourceType::BUFFER;
            entry.buffer.type = GPUBufferBindingType::STORAGE;
            return;
        case slang::TypeReflection::Kind::ConstantBuffer:
        default:
            entry.type                      = GPUResourceType::BUFFER;
            entry.buffer.type               = GPUBufferBindingType::UNIFORM;
            entry.buffer.has_dynamic_offset = false;
            entry.buffer.min_binding_size   = 0;
            return;
    }
}

void ReflectResultInternal::fill_dynamic_uniform_buffer(GPUBindGroupLayoutEntry& entry, slang::VariableLayoutReflection* var_layout)
{
    if (entry.type != GPUResourceType::BUFFER)
        return;

    auto var = var_layout->getVariable();
    if (var->findUserAttributeByName(GLOBAL_SESSION, "lyra_dynamic")) {
        entry.buffer.has_dynamic_offset = true;
        entry.buffer.min_binding_size   = var_layout->getTypeLayout()->getSize();
    }
}

GPUTextureFormat ReflectResultInternal::infer_texture_format(slang::TypeLayoutReflection* type) const
{
    auto kind = type->getKind();
    if (kind != slang::TypeReflection::Kind::Resource) {
        assert(!!!"Failed to infer texture format!");
        return GPUTextureFormat::RGBA32FLOAT;
    }

    // get the element type of the resource
    auto element_type = type->getElementTypeLayout();
    if (!element_type) {
        assert(!!!"Failed to infer texture format!");
        return GPUTextureFormat::RGBA32FLOAT;
    }

    // check the scalar type and component count
    auto scalar_type     = element_type->getScalarType();
    auto component_count = element_type->getElementCount();

    // map to common formats
    // clang-format off
    switch (scalar_type) {
        case slang::TypeReflection::ScalarType::Float32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32FLOAT;
                case 2: return GPUTextureFormat::RG32FLOAT;
                case 4: return GPUTextureFormat::RGBA32FLOAT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32UINT;
                case 2: return GPUTextureFormat::RG32UINT;
                case 4: return GPUTextureFormat::RGBA32UINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32SINT;
                case 2: return GPUTextureFormat::RG32SINT;
                case 4: return GPUTextureFormat::RGBA32SINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt16:
            switch (component_count) {
                case 1: return GPUTextureFormat::R16UINT;
                case 2: return GPUTextureFormat::RG16UINT;
                case 4: return GPUTextureFormat::RGBA16UINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int16:
            switch (component_count) {
                case 1: return GPUTextureFormat::R16SINT;
                case 2: return GPUTextureFormat::RG16SINT;
                case 4: return GPUTextureFormat::RGBA16SINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt8:
            switch (component_count) {
                case 1: return GPUTextureFormat::R8UINT;
                case 2: return GPUTextureFormat::RG8UINT;
                case 4: return GPUTextureFormat::RGBA8UINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int8:
            switch (component_count) {
                case 1: return GPUTextureFormat::R8SINT;
                case 2: return GPUTextureFormat::RG8SINT;
                case 4: return GPUTextureFormat::RGBA8SINT;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        default: break;
    }
    // clang-format on

    assert(!!!"Failed to infer texture format!");
    return GPUTextureFormat::RGBA32FLOAT;
}

GPUVertexFormat ReflectResultInternal::infer_vertex_format(slang::TypeLayoutReflection* type) const
{
    // check the scalar type and component count
    auto scalar_type     = type->getScalarType();
    auto component_count = type->getElementCount();

    // map to common formats
    // clang-format off
    switch (scalar_type) {
        case slang::TypeReflection::ScalarType::Float32:
            switch (component_count) {
                case 1: return GPUVertexFormat::FLOAT32;
                case 2: return GPUVertexFormat::FLOAT32x2;
                case 3: return GPUVertexFormat::FLOAT32x3;
                case 4: return GPUVertexFormat::FLOAT32x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt32:
            switch (component_count) {
                case 1: return GPUVertexFormat::UINT32;
                case 2: return GPUVertexFormat::UINT32x2;
                case 3: return GPUVertexFormat::UINT32x3;
                case 4: return GPUVertexFormat::UINT32x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int32:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT32;
                case 2: return GPUVertexFormat::SINT32x2;
                case 3: return GPUVertexFormat::SINT32x3;
                case 4: return GPUVertexFormat::SINT32x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt16:
            switch (component_count) {
                case 1: return GPUVertexFormat::FLOAT16;
                case 2: return GPUVertexFormat::FLOAT16x2;
                case 4: return GPUVertexFormat::FLOAT16x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int16:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT16;
                case 2: return GPUVertexFormat::SINT16x2;
                case 4: return GPUVertexFormat::SINT16x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt8:
            switch (component_count) {
                case 1: return GPUVertexFormat::UINT8;
                case 2: return GPUVertexFormat::UINT8x2;
                case 4: return GPUVertexFormat::UINT8x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int8:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT8;
                case 2: return GPUVertexFormat::SINT8x2;
                case 4: return GPUVertexFormat::SINT8x4;
                default: assert(!!!"unsupported channel count!");
            }
            break;

        default: break;
    }
    // clang-format on

    assert(!!!"Failed to infer vertex format!");
    return GPUVertexFormat::FLOAT32x4;
}

bool ReflectResultInternal::is_push_constant_buffer(const AccessPath& path) const
{
    auto node = path.leaf;
    auto type = node->var_layout->getTypeLayout();
    if (type->getParameterCategory() == slang::ParameterCategory::PushConstantBuffer)
        return true;

    auto var = node->var_layout->getVariable();
    return var->findUserAttributeByName(GLOBAL_SESSION, "vk_push_constant");
}
#pragma endregion ReflectResultInternal
