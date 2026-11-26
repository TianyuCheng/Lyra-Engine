#include "SlangReflection.h"
#include <iostream>
#include <cassert>

using namespace lyra;

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
    // check if the parameter block has an explicit lyra::group or lyra::set attribute
    auto pb_node = path.deepest_parameter_block ? path.deepest_parameter_block : path.leaf;
    if (pb_node && pb_node->var_layout) {
        if (auto var = pb_node->var_layout->getVariable()) {
            auto check_attr = [&](const char* attr_name) -> int {
                if (auto attr = var->findUserAttributeByName(get_global_session(), attr_name)) {
                    if (attr->getArgumentCount() > 0) {
                        int val = 0;
                        if (attr->getArgumentValueInt(0, &val) == SLANG_OK) {
                            return val;
                        }
                    }
                }
                return -1;
            };
            int s = check_attr("lyra_group");
            if (s < 0) s = check_attr("group");
            if (s < 0) s = check_attr("lyra_set");
            if (s < 0) s = check_attr("set");
            if (s >= 0) return static_cast<uint>(s);
        }
    }

    uint space = 0;
    switch (target) {
        case CompileTarget::MSL:
            // in Metal, there is no descriptor table, so constant buffer is used to differentiate between bindings (argument buffer),
            // since there is no notion of register spaces or descriptor set, it contributes to the value/offset, instead of space.
            space += calculate_cumulative_offset(slang::ParameterCategory::ConstantBuffer, path).value;
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
        case CompileTarget::HOST:
        case CompileTarget::AUTO:
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

    assert(!"Failed to find shader entry point!");
    return ~0u;
}

static bool contains_ordinary_types(slang::TypeLayoutReflection* type_layout)
{
    if (!type_layout) return false;
    auto kind = type_layout->getKind();
    switch (kind) {
        case slang::TypeReflection::Kind::Scalar:
        case slang::TypeReflection::Kind::Vector:
        case slang::TypeReflection::Kind::Matrix:
            return true;
        case slang::TypeReflection::Kind::Array:
            return contains_ordinary_types(type_layout->getElementTypeLayout());
        case slang::TypeReflection::Kind::Struct:
        {
            uint count = type_layout->getFieldCount();
            for (uint i = 0; i < count; i++) {
                if (contains_ordinary_types(type_layout->getFieldByIndex(i)->getTypeLayout()))
                    return true;
            }
            return false;
        }
        default:
            return false;
    }
}

#if defined(SLANG_DEBUG)
static void print_slang_var_layout(const Vector<EntryMetadata>& metadata, const AccessPath& curr, slang::VariableLayoutReflection* var_layout, const TraversalData& data)
{
    int  walk_depth  = data.current_walk_depth;
    auto print_depth = [&](CString prefix = "  ") -> std::ostream& {
        for (int i = 0; i < walk_depth; i++)
            std::cout << prefix;
        return std::cout;
    };
    print_depth() << "# PATH: " << curr.to_string() << std::endl;
    if (var_layout->getName())
        print_depth() << "# NAME: " << var_layout->getName() << std::endl;
    else
        print_depth() << "# NAME: unknown" << std::endl;

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
#endif // SLANG_DEBUG

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

bool ReflectResultInternal::get_immediate_size(uint& size) const
{
    if (has_error) return false;
    size = immediate_size;
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
            if (typ_layout->getKind() == slang::TypeReflection::Kind::ParameterBlock ||
                container_var_layout->getTypeLayout()->getSize(slang::ParameterCategory::SubElementRegisterSpace) != 0)
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
                // parameter block has a name, we record it for bind group retrieval
                record_parameter_block_space(path);

                // parameter block will automatically introduce a constant buffer binding if ordinary types are observed.
                if (typ_layout->getElementTypeLayout()->getSize() ||
                    contains_ordinary_types(typ_layout->getElementTypeLayout()))
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

    auto global_params = program_layout->getGlobalParamsVarLayout();
    uint global_fields = global_params && global_params->getTypeLayout() ? global_params->getTypeLayout()->getFieldCount() : 0;

    if (global_fields > 0) {
        walk(global_params, AccessPath{}, callback);
    } else {
        for (uint i = 0; i < program_layout->getParameterCount(); ++i) {
            auto param_var = program_layout->getParameterByIndex(i);
            if (param_var) {
                walk(param_var, AccessPath{}, callback);
            }
        }
    }

    if (target != CompileTarget::HOST) {
        // iterate through entry points
        auto entry_point_count = program_layout->getEntryPointCount();
        for (uint i = 0; i < entry_point_count; i++) {
            auto entry_point_reflect = program_layout->getEntryPointByIndex(i);
            walk(entry_point_reflect, AccessPath{}, callback);
        }
    }
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

                semantic_names.push_back(semantic_name);

                vertex_attributes.push_back(GPUVertexAttribute{});
                auto& attribute           = vertex_attributes.back();
                attribute.offset          = 0; // host-provided, cannot be reflected from shader
                attribute.format          = infer_vertex_format(typ_layout);
                attribute.shader_semantic = semantic_names.back().c_str();
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

    if (target == CompileTarget::HOST && vertex_attributes.empty()) {
        for (uint i = 0; i < entry_point_count; ++i) {
            auto ep = program_layout->getEntryPointByIndex(i);
            if (!ep) continue;
            String ep_name = ep->getName() ? ep->getName() : "";
            if (ep_name.find("vs") != String::npos || ep_name.find("vert") != String::npos) {
                if (ep->getParameterCount() > 0) {
                    auto param      = ep->getParameterByIndex(0);
                    auto param_type = param ? param->getTypeLayout() : nullptr;
                    if (param_type) {
                        auto struct_type = param_type->getElementTypeLayout() ? param_type->getElementTypeLayout() : param_type;
                        uint field_count = struct_type->getFieldCount();
                        for (uint f = 0; f < field_count; ++f) {
                            auto field = struct_type->getFieldByIndex(f);
                            if (!field) continue;
                            auto field_name = field->getName() ? field->getName() : "";
                            auto sem_name   = field->getSemanticName() ? field->getSemanticName() : "";
                            semantic_names.push_back(sem_name);

                            vertex_attributes.push_back(GPUVertexAttribute{});
                            auto& attribute           = vertex_attributes.back();
                            attribute.offset          = 0;
                            attribute.format          = infer_vertex_format(field->getTypeLayout());
                            attribute.shader_semantic = semantic_names.back().c_str();
                            attribute.shader_location = f;

                            name2attributes.emplace(field_name, static_cast<uint>(vertex_attributes.size() - 1));
                        }
                    }
                }
            }
        }
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
    fill_binding_index(val, offset, path);
    fill_binding_stages(val, path);
    fill_dynamic_uniform_buffer(val, node->var_layout);

    if (is_immediates_buffer(path)) {
        create_immediates(path, offset, val);
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
    fill_binding_index(val, offset, path);
    fill_binding_count(val, type);
    fill_binding_stages(val, path);
    fill_dynamic_uniform_buffer(val, node->var_layout);

    // check for immediates vs constant buffer view binding
    if (is_immediates_buffer(path)) {
        create_immediates(path, offset, val);
    } else {
        bind_groups[offset.space].push_back(val);
        get_logger()->trace("[BINDGROUP] NAME:{}\t SPACE:{} BINDING:{}", node->var_layout->getName(), offset.space, offset.value);
    }
}

void ReflectResultInternal::create_immediates(const AccessPath& path, const CumulativeOffset& offset, const GPUBindGroupLayoutEntry& binding)
{
    auto node = path.leaf;

    // enforce that immediates constant buffer is in the expected space/slot.
    if (target == CompileTarget::DXIL && offset.space != D3D12_ImmediateRegisterSpace) {
        get_logger()->error("Please use [[lyra::immediates]] to annotate immediates, found space {}, expected {}", offset.space, D3D12_ImmediateRegisterSpace);
        has_error = true;
        return;
    }

    // enforce that immediates constant buffer is in the expected space/slot.
    if (target == CompileTarget::MSL && offset.space != METAL_ImmediateBufferIndex) {
        get_logger()->error("Please use [[lyra::immediates]] to annotate immediates, found buffer slot {}, expected {}", offset.space, METAL_ImmediateBufferIndex);
        has_error = true;
        return;
    }

    if (node->var_layout->getType()->getKind() != slang::TypeReflection::Kind::ConstantBuffer) {
        get_logger()->error("Please directly define immediates using ConstantBuffer<T>.");
        has_error = true;
        return;
    }

    if (num_immediate_buffers++ >= 1) {
        get_logger()->error("Please specify all immediates using only one ConstantBuffer<T>. Using multiple ConstantBuffer<T> is not allowed!");
        has_error = true;
        return;
    }

    auto imm_type  = node->var_layout->getTypeLayout()->getElementTypeLayout();
    immediate_size = static_cast<uint>(imm_type->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
    get_logger()->trace("[IMMEDIATES] NAME:{}\t SIZE:{}",
        node->var_layout->getName(),
        immediate_size);
}

void ReflectResultInternal::fill_binding_index(GPUBindGroupLayoutEntry& entry, CumulativeOffset offset, const AccessPath& path) const
{
    entry.binding.index = offset.value;

    if (path.leaf && path.leaf->var_layout) {
        if (auto var = path.leaf->var_layout->getVariable()) {
            auto check_binding = [&](const char* attr_name) -> int {
                if (auto attr = var->findUserAttributeByName(get_global_session(), attr_name)) {
                    if (attr->getArgumentCount() > 0) {
                        int val = 0;
                        if (attr->getArgumentValueInt(0, &val) == SLANG_OK) {
                            return val;
                        }
                    }
                }
                return -1;
            };
            int b = check_binding("lyra_binding");
            if (b < 0) b = check_binding("binding");
            if (b >= 0) {
                entry.binding.index = static_cast<uint>(b);
            }
        }
    }

    // metal supports binding the resource directly, or indirectly via argument buffer.
    // this cannot be derived from later code path, so we must record it now.
    if (target == CompileTarget::MSL) {
        entry.binding.from_argument_buffer = is_under_parameter_block(path);
    }
}

void ReflectResultInternal::fill_binding_count(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const
{
    if (!type->isArray()) {
        entry.count = 1;
        return;
    }

    usize count = type->getTotalArrayElementCount();
    entry.count = static_cast<uint>(count);
}

void ReflectResultInternal::fill_binding_stages(GPUBindGroupLayoutEntry& entry, const AccessPath& path) const
{
    if (target == CompileTarget::MSL)
        return;

    entry.visibility = calculate_shader_stage_mask(metadata, path);
}

void ReflectResultInternal::fill_binding_type(GPUBindGroupLayoutEntry& entry, slang::TypeLayoutReflection* type) const
{
    auto kind = type->getKind();

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
                case SLANG_TEXTURE_1D_ARRAY:   assert(!"TEXTURE1D_ARRAY is not supported!");       break;
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
                    entry.type        = GPUResourceType::BUFFER;
                    entry.buffer.type = (access_permission != GPUStorageTextureAccess::READ_ONLY)
                                            ? GPUBufferBindingType::STORAGE
                                            : GPUBufferBindingType::READ_ONLY_STORAGE;

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
    if (var->findUserAttributeByName(get_global_session(), "lyra_dynamic")) {
        entry.buffer.has_dynamic_offset = true;
        entry.buffer.min_binding_size   = var_layout->getTypeLayout()->getSize();
    }
}

GPUTextureFormat ReflectResultInternal::infer_texture_format(slang::TypeLayoutReflection* type) const
{
    auto kind = type->getKind();
    if (kind != slang::TypeReflection::Kind::Resource) {
        assert(!"Failed to infer texture format!");
        return GPUTextureFormat::RGBA32FLOAT;
    }

    auto result_type = type->getResourceResultType();
    if (!result_type) {
        assert(!"Failed to infer texture format!");
        return GPUTextureFormat::RGBA32FLOAT;
    }

    auto scalar_type     = result_type->getScalarType();
    auto component_count = result_type->getElementCount();

    // clang-format off
    switch (scalar_type) {
        case slang::TypeReflection::ScalarType::Float32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32FLOAT;
                case 2: return GPUTextureFormat::RG32FLOAT;
                case 4: return GPUTextureFormat::RGBA32FLOAT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32UINT;
                case 2: return GPUTextureFormat::RG32UINT;
                case 4: return GPUTextureFormat::RGBA32UINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int32:
            switch (component_count) {
                case 1: return GPUTextureFormat::R32SINT;
                case 2: return GPUTextureFormat::RG32SINT;
                case 4: return GPUTextureFormat::RGBA32SINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt16:
            switch (component_count) {
                case 1: return GPUTextureFormat::R16UINT;
                case 2: return GPUTextureFormat::RG16UINT;
                case 4: return GPUTextureFormat::RGBA16UINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int16:
            switch (component_count) {
                case 1: return GPUTextureFormat::R16SINT;
                case 2: return GPUTextureFormat::RG16SINT;
                case 4: return GPUTextureFormat::RGBA16SINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt8:
            switch (component_count) {
                case 1: return GPUTextureFormat::R8UINT;
                case 2: return GPUTextureFormat::RG8UINT;
                case 4: return GPUTextureFormat::RGBA8UINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int8:
            switch (component_count) {
                case 1: return GPUTextureFormat::R8SINT;
                case 2: return GPUTextureFormat::RG8SINT;
                case 4: return GPUTextureFormat::RGBA8SINT;
                default: assert(!"unsupported channel count!");
            }
            break;

        default: break;
    }
    // clang-format on

    assert(!"Failed to infer texture format!");
    return GPUTextureFormat::RGBA32FLOAT;
}

GPUVertexFormat ReflectResultInternal::infer_vertex_format(slang::TypeLayoutReflection* type) const
{
    auto scalar_type     = type->getScalarType();
    auto component_count = (type->getKind() == slang::TypeReflection::Kind::Scalar) ? 1 : type->getElementCount();
    if (component_count == 0) {
        component_count = 1;
    }

    // clang-format off
    switch (scalar_type) {
        case slang::TypeReflection::ScalarType::Float32:
            switch (component_count) {
                case 1: return GPUVertexFormat::FLOAT32;
                case 2: return GPUVertexFormat::FLOAT32x2;
                case 3: return GPUVertexFormat::FLOAT32x3;
                case 4: return GPUVertexFormat::FLOAT32x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt32:
            switch (component_count) {
                case 1: return GPUVertexFormat::UINT32;
                case 2: return GPUVertexFormat::UINT32x2;
                case 3: return GPUVertexFormat::UINT32x3;
                case 4: return GPUVertexFormat::UINT32x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int32:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT32;
                case 2: return GPUVertexFormat::SINT32x2;
                case 3: return GPUVertexFormat::SINT32x3;
                case 4: return GPUVertexFormat::SINT32x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt16:
            switch (component_count) {
                case 1: return GPUVertexFormat::UINT16;
                case 2: return GPUVertexFormat::UINT16x2;
                case 4: return GPUVertexFormat::UINT16x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int16:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT16;
                case 2: return GPUVertexFormat::SINT16x2;
                case 4: return GPUVertexFormat::SINT16x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::UInt8:
            switch (component_count) {
                case 1: return GPUVertexFormat::UINT8;
                case 2: return GPUVertexFormat::UINT8x2;
                case 4: return GPUVertexFormat::UINT8x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        case slang::TypeReflection::ScalarType::Int8:
            switch (component_count) {
                case 1: return GPUVertexFormat::SINT8;
                case 2: return GPUVertexFormat::SINT8x2;
                case 4: return GPUVertexFormat::SINT8x4;
                default: assert(!"unsupported channel count!");
            }
            break;

        default: break;
    }
    // clang-format on

    assert(!"Failed to infer vertex format!");
    return GPUVertexFormat::FLOAT32x4;
}

bool ReflectResultInternal::is_immediates_buffer(const AccessPath& path) const
{
    auto node = path.leaf;
    auto type = node->var_layout->getTypeLayout();
    if (type->getParameterCategory() == slang::ParameterCategory::PushConstantBuffer)
        return true;

    auto var = node->var_layout->getVariable();
    return var->findUserAttributeByName(get_global_session(), "lyra_immediates") != nullptr ||
           var->findUserAttributeByName(get_global_session(), "immediates") != nullptr;
}

bool ReflectResultInternal::is_under_parameter_block(const AccessPath& node) const
{
    for (auto leaf = node.leaf; leaf != nullptr; leaf = leaf->outer) {
        auto typ_layout = leaf->var_layout->getTypeLayout();
        if (typ_layout->getKind() == slang::TypeReflection::Kind::ParameterBlock)
            return true;
    }
    return false;
}
