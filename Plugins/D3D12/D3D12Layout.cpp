// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"
#include "d3d12.h"

uint round_up_to_multiple_of(uint size, uint align)
{
    return (size + align - 1) / align * align;
}

bool is_dynamic_bind_group_entry(const GPUBindGroupLayoutEntry& entry)
{
    return entry.type == GPUResourceType::BUFFER
               ? entry.buffer.has_dynamic_offset
               : false;
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_buffer_descriptor_type(const GPUBufferBindingLayout& entry)
{
    switch (entry.type) {
        case GPUBufferBindingType::STORAGE:
        case GPUBufferBindingType::READ_ONLY_STORAGE:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case GPUBufferBindingType::UNIFORM:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        default:
            throw std::invalid_argument("Invalid GPUBufferBindingType!");
    }
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_sampler_descriptor_type(const GPUSamplerBindingLayout&)
{
    return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_texture_descriptor_type(const GPUTextureBindingLayout&)
{
    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_storage_texture_descriptor_type(const GPUStorageTextureBindingLayout&)
{
    return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_bvh_descriptor_type(const GPUBVHBindingLayout&)
{
    return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
}

D3D12_DESCRIPTOR_RANGE_TYPE infer_descriptor_type(const GPUBindGroupLayoutEntry& entry)
{
    switch (entry.type) {
        case GPUResourceType::BUFFER:
            return infer_buffer_descriptor_type(entry.buffer);
        case GPUResourceType::SAMPLER:
            return infer_sampler_descriptor_type(entry.sampler);
        case GPUResourceType::TEXTURE:
            return infer_texture_descriptor_type(entry.texture);
        case GPUResourceType::STORAGE_TEXTURE:
            return infer_storage_texture_descriptor_type(entry.storage_texture);
        case GPUResourceType::ACCELERATION_STRUCTURE:
            return infer_bvh_descriptor_type(entry.bvh);
        default:
            throw std::invalid_argument("Unsupported GPU binding resource type!");
    }
}

#pragma region D3D12PipelineLayout
D3D12PipelineLayout::D3D12PipelineLayout()
{
    layout = nullptr;
}

D3D12PipelineLayout::D3D12PipelineLayout(const GPUPipelineLayoutDescriptor& desc)
{
    layout = nullptr;

    auto rhi = get_rhi();

    uint root_parameter_index = 0;
    uint register_space_index = 0;

    Vector<D3D12_ROOT_PARAMETER1> root_parameters;
    bindgroups.resize(desc.bind_group_layouts.size());
    root_parameters.reserve(desc.bind_group_layouts.size() * 2);

    // regular bindings
    for (auto& layout_handle : desc.bind_group_layouts) {
        auto& layout = fetch_resource(rhi->bind_group_layouts, layout_handle);

        auto& bindgroup = bindgroups.at(register_space_index);

        // fix the register space
        for (auto& range : layout.default_ranges)
            range.RegisterSpace = register_space_index;

        // fix the register space
        for (auto& range : layout.sampler_ranges)
            range.RegisterSpace = register_space_index;

        // add default ranges
        if (!layout.default_ranges.empty()) {
            root_parameters.push_back({});
            auto& root_param                               = root_parameters.back();
            root_param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            root_param.ShaderVisibility                    = layout.visibility;
            root_param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(layout.default_ranges.size());
            root_param.DescriptorTable.pDescriptorRanges   = layout.default_ranges.data();
            bindgroup.default_root_parameter               = root_parameter_index++;
        }

        // add sampler ranges
        if (!layout.sampler_ranges.empty()) {
            root_parameters.push_back({});
            auto& root_param                               = root_parameters.back();
            root_param.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            root_param.ShaderVisibility                    = layout.visibility;
            root_param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(layout.sampler_ranges.size());
            root_param.DescriptorTable.pDescriptorRanges   = layout.sampler_ranges.data();
            bindgroup.sampler_root_parameter               = root_parameter_index++;
        }

        // add dynamic ranges
        if (!layout.dynamic_ranges.empty()) {
            bindgroup.dynamic_root_parameter = root_parameter_index++;
            for (auto& range : layout.dynamic_ranges) {
                root_parameters.push_back({});
                auto& root_param                     = root_parameters.back();
                root_param.ParameterType             = range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV ? D3D12_ROOT_PARAMETER_TYPE_CBV : D3D12_ROOT_PARAMETER_TYPE_UAV;
                root_param.ShaderVisibility          = layout.visibility;
                root_param.Descriptor.ShaderRegister = range.BaseShaderRegister;
                root_param.Descriptor.RegisterSpace  = range.RegisterSpace;
                root_param.Descriptor.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
            }
        }

        register_space_index++;
    }

    // root constants
    if (!desc.push_constant_ranges.empty()) {
        auto visibility = GPUShaderStageFlags(0);
        for (auto& range : desc.push_constant_ranges)
            visibility = visibility | range.visibility;

        uint bytes = 0;
        for (auto& range : desc.push_constant_ranges)
            bytes = std::max(bytes, range.offset + range.size);

        root_parameters.push_back({});
        auto& root_param                    = root_parameters.back();
        root_param.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        root_param.ShaderVisibility         = d3d12enum(visibility);
        root_param.Constants.ShaderRegister = 0;
        root_param.Constants.RegisterSpace  = D3D12_PushConstantRegisterSpace;
        root_param.Constants.Num32BitValues = round_up_to_multiple_of(bytes, 4) / sizeof(uint32_t);

        this->push_constant_root_parameter = root_parameter_index++;
    }

    // create root signature descriptor
    D3D12_ROOT_SIGNATURE_DESC1 root_signature_desc = {};
    root_signature_desc.NumParameters              = static_cast<UINT>(root_parameters.size());
    root_signature_desc.pParameters                = root_parameters.data();
    root_signature_desc.NumStaticSamplers          = 0;
    root_signature_desc.pStaticSamplers            = nullptr;
    root_signature_desc.Flags                      = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // use versioned root signature for better performance
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc = {};
    versioned_desc.Version                             = D3D_ROOT_SIGNATURE_VERSION_1_1;
    versioned_desc.Desc_1_1                            = root_signature_desc;

    ID3DBlob* error_blob = nullptr;
    ID3DBlob* signature  = nullptr;
    HRESULT   hr         = D3D12SerializeVersionedRootSignature(&versioned_desc, &signature, &error_blob);
    if (FAILED(hr)) {
        if (error_blob) {
            const char* error_msg = static_cast<const char*>(error_blob->GetBufferPointer());
            get_logger()->error("{}", error_msg);
            error_blob->Release();
            error_blob = nullptr;
        }

        if (signature) {
            signature->Release();
            signature = nullptr;
        }
    }

    if (error_blob)
        error_blob->Release();

    if (FAILED(hr)) return;

    ThrowIfFailed(rhi->device->CreateRootSignature(0, signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&layout)));

    if (desc.label)
        layout->SetName(to_wstring(desc.label).c_str());
}

void D3D12PipelineLayout::destroy()
{
    if (layout) {
        layout->Release();
        layout = nullptr;
    }
}

void D3D12PipelineLayout::create_dispatch_indirect_signature()
{
    if (signatures.dispatch_indirect) return;

    D3D12_INDIRECT_ARGUMENT_DESC args[1];
    args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

    D3D12_COMMAND_SIGNATURE_DESC program_desc{};
    program_desc.ByteStride       = 12;
    program_desc.NumArgumentDescs = 1;
    program_desc.pArgumentDescs   = args;

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->device->CreateCommandSignature(&program_desc, layout, IID_PPV_ARGS(&signatures.dispatch_indirect)));
}

void D3D12PipelineLayout::create_draw_indirect_signature()
{
    if (signatures.draw_indirect) return;

    D3D12_INDIRECT_ARGUMENT_DESC args[1];
    args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

    D3D12_COMMAND_SIGNATURE_DESC program_desc{};
    program_desc.ByteStride       = 16;
    program_desc.NumArgumentDescs = 1;
    program_desc.pArgumentDescs   = args;

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->device->CreateCommandSignature(&program_desc, layout, IID_PPV_ARGS(&signatures.draw_indirect)));
}

void D3D12PipelineLayout::create_draw_indexed_indirect_signature()
{
    if (signatures.draw_indexed_indirect) return;

    D3D12_INDIRECT_ARGUMENT_DESC args[1];
    args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC program_desc{};
    program_desc.ByteStride       = 20;
    program_desc.NumArgumentDescs = 1;
    program_desc.pArgumentDescs   = args;

    auto rhi = get_rhi();
    ThrowIfFailed(rhi->device->CreateCommandSignature(&program_desc, layout, IID_PPV_ARGS(&signatures.draw_indexed_indirect)));
}

#pragma endregion D3D12PipelineLayout

#pragma region D3D12BindGroupLayout
D3D12BindGroupLayout::D3D12BindGroupLayout()
{
    default_ranges.clear();
    sampler_ranges.clear();
    visibility = D3D12_SHADER_VISIBILITY_ALL;
    bindless   = false;
}

D3D12BindGroupLayout::D3D12BindGroupLayout(const GPUBindGroupLayoutDescriptor& desc)
{
    // bindless = desc.bindless;

    // determine overall shader visibility
    visibility = D3D12_SHADER_VISIBILITY_ALL;

    // record requires stages
    GPUShaderStageFlags stages = 0;

    // check binding type (D3D12 has requirement that sampler cannot be mixed with others)
    uint binding_count = static_cast<uint>(desc.entries.size());
    uint sampler_count = 0;
    uint default_count = 0;
    uint dynamic_count = 0;
    for (const auto& entry : desc.entries) {
        if (entry.type == GPUResourceType::SAMPLER)
            sampler_count++;
        else if (is_dynamic_bind_group_entry(entry))
            dynamic_count++;
        else
            default_count++;
    }

    // reserve enough space for ranges
    sampler_ranges.reserve(sampler_count);
    default_ranges.reserve(default_count);
    dynamic_ranges.reserve(dynamic_count);
    bindings.reserve(binding_count);

    // initialize ranges vector based on descriptor entries
    for (const auto& entry : desc.entries) {
        D3D12_DESCRIPTOR_RANGE1 range{};

        // range info
        range.BaseShaderRegister                = entry.binding.index;
        range.NumDescriptors                    = entry.count;
        range.RegisterSpace                     = 0; // NOTE: need to be changed later in the pipeline layout
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        range.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
        range.RangeType                         = infer_descriptor_type(entry);

        // additional flag for read-only buffer resources
        if (entry.type == GPUResourceType::BUFFER)
            if (entry.buffer.type == GPUBufferBindingType::READ_ONLY_STORAGE)
                range.Flags |= D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // save this range into corresponding ranges
        if (entry.type == GPUResourceType::SAMPLER)
            sampler_ranges.push_back(range);
        else if (is_dynamic_bind_group_entry(entry))
            dynamic_ranges.push_back(range);
        else
            default_ranges.push_back(range);

        // update shader visibility
        stages = stages | entry.visibility;

        // populate binding info
        auto binding    = D3D12BindInfo{};
        binding.type    = range.RangeType;
        binding.count   = entry.count;
        binding.dynamic = is_dynamic_bind_group_entry(entry);
        bindings.push_back(binding);
    }

    // populate default descriptor counts
    this->num_defaults = 0;
    for (auto& range : default_ranges)
        this->num_defaults += range.NumDescriptors;

    // populate sampler descriptor counts
    this->num_samplers = 0;
    for (auto& range : sampler_ranges)
        this->num_samplers += range.NumDescriptors;

    // populate dynamic descriptor counts
    this->num_dynamics = 0;
    for (auto& range : dynamic_ranges)
        this->num_dynamics += range.NumDescriptors;

    // populate binding offset
    uint default_offset = 0;
    uint sampler_offset = 0;
    for (auto& binding : bindings) {
        if (binding.type == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) {
            binding.start = sampler_offset;
            sampler_offset += binding.count;
        } else {
            binding.start = default_offset;
            default_offset += binding.count;
        }
    }

    // optimize shader visibility if possible
    visibility = d3d12enum(stages);
}

void D3D12BindGroupLayout::destroy()
{
    default_ranges.clear();
    sampler_ranges.clear();
    dynamic_ranges.clear();
    visibility = D3D12_SHADER_VISIBILITY_ALL;
    bindless   = false;
}

D3D12BindGroup* D3D12BindGroupLayout::create(GPUBindGroupHeapHandle heap_handle, const GPUBindGroupDescriptor& desc)
{
    assert(!bindless && "Cannot create bindless descriptor using bound descriptor entries!");
    auto rhi = get_rhi();

    // allocate descriptors
    auto& heap       = fetch_resource(rhi->bind_group_heaps, heap_handle);
    auto  bind_group = heap.memory->allocate<D3D12BindGroup>();

    bind_group->default_index = std::numeric_limits<uint32_t>::max();
    bind_group->sampler_index = std::numeric_limits<uint32_t>::max();
    bind_group->dynamic_index = std::numeric_limits<uint16_t>::max();
    bind_group->heap          = heap_handle;

    // allocate descriptors for default ranges
    if (num_defaults)
        bind_group->default_index = heap.default_heap.allocate(num_defaults);

    // allocate descriptors for sampler ranges
    if (num_samplers)
        bind_group->sampler_index = heap.sampler_heap.allocate(num_samplers);

    // write to descriptors
    for (auto& entry : desc.entries) {
        auto& bind_info = bindings.at(entry.binding);
        copy_regular_descriptors(heap, entry, bind_info, *bind_group);
    }

    return bind_group;
}

void D3D12BindGroupLayout::copy_regular_descriptors(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    switch (entry.type) {
        case GPUResourceType::SAMPLER:
            copy_sampler_descriptor(heap, entry, bind_info, bind_group);
            break;
        case GPUResourceType::BUFFER:
            create_buffer_descriptor(heap, entry, bind_info, bind_group);
            break;
        case GPUResourceType::TEXTURE:
        case GPUResourceType::STORAGE_TEXTURE:
            copy_texture_descriptor(heap, entry, bind_info, bind_group);
            break;
        case GPUResourceType::ACCELERATION_STRUCTURE:
            create_bvh_descriptor(heap, entry, bind_info, bind_group);
            break;
        default:
            assert(!!!"Invaid GPUResourceType");
    }
}

void D3D12BindGroupLayout::copy_sampler_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto  rhi = get_rhi();
    auto& smp = fetch_resource(rhi->samplers, entry.sampler);

    D3D12_CPU_DESCRIPTOR_HANDLE src_handle = smp.sampler.handle;
    D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = rhi->gpu_sampler_heap.cpu(bind_group.sampler_index + bind_info.start);
    rhi->device->CopyDescriptorsSimple(1, dst_handle, src_handle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void D3D12BindGroupLayout::copy_texture_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto  rhi = get_rhi();
    auto& tex = fetch_resource(rhi->views, entry.texture);

    D3D12_CPU_DESCRIPTOR_HANDLE src_handle = entry.type == GPUResourceType::TEXTURE ? tex.srv_view.handle : tex.uav_view.handle;
    D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = rhi->gpu_default_heap.cpu(bind_group.default_index + bind_info.start);
    rhi->device->CopyDescriptorsSimple(1, dst_handle, src_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3D12BindGroupLayout::create_buffer_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto type = bindings.at(entry.binding).type;
    switch (type) {
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            create_buffer_cbv_descriptor(heap, entry, bind_info, bind_group);
            break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            create_buffer_uav_descriptor(heap, entry, bind_info, bind_group);
            break;
        default:
            assert(!!!"Buffer descriptor only supports CBV and UAV!");
    }
}

void D3D12BindGroupLayout::create_buffer_cbv_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto  rhi = get_rhi();
    auto& buf = fetch_resource(rhi->buffers, entry.buffer.buffer);

    if (bind_info.dynamic) {
        uint  index    = heap.dynamic_heap.allocate();
        auto& object   = heap.dynamic_heap.at(index);
        object.type    = D3D12_ROOT_PARAMETER_TYPE_CBV;
        object.address = buf.buffer->GetGPUVirtualAddress() + entry.buffer.offset;
        if (bind_group.dynamic_index == std::numeric_limits<uint16_t>::max())
            bind_group.dynamic_index = index;
    } else {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
        cbv_desc.BufferLocation = buf.buffer->GetGPUVirtualAddress() + entry.buffer.offset;
        cbv_desc.SizeInBytes    = static_cast<uint>(entry.buffer.size == 0 ? buf.size() : entry.buffer.size);
        cbv_desc.SizeInBytes    = (cbv_desc.SizeInBytes + 255) & ~255; // CBV alignment

        D3D12_CPU_DESCRIPTOR_HANDLE descriptor = rhi->gpu_default_heap.cpu(bind_group.default_index + bind_info.start);
        rhi->device->CreateConstantBufferView(&cbv_desc, descriptor);
    }
}

void D3D12BindGroupLayout::create_buffer_uav_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto  rhi = get_rhi();
    auto& buf = fetch_resource(rhi->buffers, entry.buffer.buffer);

    if (bind_info.dynamic) {
        uint  index    = heap.dynamic_heap.allocate();
        auto& object   = heap.dynamic_heap.at(index);
        object.type    = D3D12_ROOT_PARAMETER_TYPE_UAV;
        object.address = buf.buffer->GetGPUVirtualAddress() + entry.buffer.offset;
        if (bind_group.dynamic_index == std::numeric_limits<uint16_t>::max())
            bind_group.dynamic_index = index;
    } else {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format                      = DXGI_FORMAT_R8_UINT;
        uav_desc.ViewDimension               = D3D12_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.FirstElement         = entry.buffer.offset;
        uav_desc.Buffer.NumElements          = static_cast<uint>(entry.buffer.size == 0 ? buf.size() : entry.buffer.size);
        uav_desc.Buffer.StructureByteStride  = 1;
        uav_desc.Buffer.CounterOffsetInBytes = 0;
        uav_desc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;

        D3D12_CPU_DESCRIPTOR_HANDLE descriptor = rhi->gpu_default_heap.cpu(bind_group.default_index + bind_info.start);
        rhi->device->CreateUnorderedAccessView(buf.buffer, nullptr, &uav_desc, descriptor);
    }
}

void D3D12BindGroupLayout::create_bvh_descriptor(D3D12BindGroupHeap& heap, const GPUBindGroupEntry& entry, const D3D12BindInfo& bind_info, D3D12BindGroup& bind_group)
{
    auto  rhi = get_rhi();
    auto& acc = fetch_resource(rhi->tlases, entry.tlas);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc          = {};
    srv_desc.Format                                   = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srv_desc.Shader4ComponentMapping                  = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.RaytracingAccelerationStructure.Location = acc.tlas->GetGPUVirtualAddress();

    D3D12_CPU_DESCRIPTOR_HANDLE descriptor = rhi->gpu_default_heap.cpu(bind_group.default_index + bind_info.start);
    rhi->device->CreateShaderResourceView(nullptr, &srv_desc, descriptor);
}
#pragma endregion D3D12BindGroupLayout
