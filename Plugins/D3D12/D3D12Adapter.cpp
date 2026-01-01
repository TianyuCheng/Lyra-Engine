// reference: https://alain.xyz/blog/raw-directx12
#include "D3D12Utils.h"

void populate_adapter_properties(GPUSupportedLimits& limits, GPUProperties& properties)
{
    ComPtr<ID3D12Device> device;
    ThrowIfFailed(D3D12CreateDevice(get_rhi()->adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

    // query d3d12 feature data
    D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12_options = {};
    device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &d3d12_options, sizeof(d3d12_options));

    D3D12_FEATURE_DATA_GPU_VIRTUAL_ADDRESS_SUPPORT virtual_address_support = {};
    device->CheckFeatureSupport(D3D12_FEATURE_GPU_VIRTUAL_ADDRESS_SUPPORT, &virtual_address_support, sizeof(virtual_address_support));

    D3D12_FEATURE_DATA_SHADER_MODEL shader_model = {};
    shader_model.HighestShaderModel              = D3D_SHADER_MODEL_6_6;
    device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shader_model, sizeof(shader_model));

    // texture dimension limits (d3d12 hardware limits)
    limits.max_texture_dimension_1d = D3D12_REQ_TEXTURE1D_U_DIMENSION;          // 16384
    limits.max_texture_dimension_2d = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;     // 16384
    limits.max_texture_dimension_3d = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;   // 2048
    limits.max_texture_array_layers = D3D12_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION; // 2048

    // descriptor heap limits (root signature has max 64 entries)
    limits.max_bind_groups             = 4;  // conservative estimate for webgpu compatibility
    limits.max_bindings_per_bind_group = 16; // conservative for root signature limits

    // dynamic buffer limits
    limits.max_dynamic_uniform_buffers_per_pipeline_layout = 8;
    limits.max_dynamic_storage_buffers_per_pipeline_layout = 4;

    // per-shader stage resource limits
    limits.max_sampled_textures_per_shader_stage = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; // 128
    limits.max_samplers_per_shader_stage         = D3D12_COMMONSHADER_SAMPLER_SLOT_COUNT;        // 16
    limits.max_storage_buffers_per_shader_stage  = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; // 128
    limits.max_storage_textures_per_shader_stage = D3D12_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; // 128
    limits.max_uniform_buffers_per_shader_stage  = 14;                                           // d3d12 supports 14 constant buffer slots (b0-b13)

    // buffer size and alignment limits
    limits.max_uniform_buffer_binding_size     = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16; // 4096 * 16 = 65536
    limits.max_storage_buffer_binding_size     = static_cast<uint32_t>(std::min(
        static_cast<uint64_t>(UINT32_MAX),
        virtual_address_support.MaxGPUVirtualAddressBitsPerResource > 0 ? (1ULL << virtual_address_support.MaxGPUVirtualAddressBitsPerResource) : static_cast<uint64_t>(128 * 1024 * 1024) // 128mb fallback
        ));
    limits.min_uniform_buffer_offset_alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256
    limits.min_storage_buffer_offset_alignment = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT;               // 16

    // vertex input limits
    limits.max_vertex_buffers             = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; // 32
    limits.max_buffer_size                = static_cast<uint32_t>(std::min(
        static_cast<uint64_t>(UINT32_MAX),
        virtual_address_support.MaxGPUVirtualAddressBitsPerResource > 0 ? (1ULL << virtual_address_support.MaxGPUVirtualAddressBitsPerResource) : static_cast<uint64_t>(128 * 1024 * 1024) // 128mb fallback
        ));
    limits.max_vertex_attributes          = D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT; // 32
    limits.max_vertex_buffer_array_stride = D3D12_SO_BUFFER_MAX_STRIDE_IN_BYTES;           // 2048

    // inter-stage variables (input/output registers)
    limits.max_inter_stage_shader_variables = 32; // d3d12 supports 32 input/output registers

    // render target limits
    limits.max_color_attachments                 = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; // 8
    limits.max_color_attachment_bytes_per_sample = 32;                                     // rgba32 format

    // compute shader limits
    limits.max_compute_workgroup_storage_size    = D3D12_CS_THREAD_LOCAL_TEMP_REGISTER_POOL;          // 16384
    limits.max_compute_invocations_per_workgroup = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;       // 1024
    limits.max_compute_workgroup_size_x          = D3D12_CS_THREAD_GROUP_MAX_X;                       // 1024
    limits.max_compute_workgroup_size_y          = D3D12_CS_THREAD_GROUP_MAX_Y;                       // 1024
    limits.max_compute_workgroup_size_z          = D3D12_CS_THREAD_GROUP_MAX_Z;                       // 64
    limits.max_compute_workgroups_per_dimension  = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION; // 65535

    // clamp values to webgpu-compatible ranges for better compatibility
    limits.max_sampled_textures_per_shader_stage = std::min(limits.max_sampled_textures_per_shader_stage, 16u);
    limits.max_storage_buffers_per_shader_stage  = std::min(limits.max_storage_buffers_per_shader_stage, 8u);
    limits.max_storage_textures_per_shader_stage = std::min(limits.max_storage_textures_per_shader_stage, 4u);
    limits.max_uniform_buffers_per_shader_stage  = std::min(limits.max_uniform_buffers_per_shader_stage, 12u);
    limits.max_vertex_buffers                    = std::min(limits.max_vertex_buffers, 8u);
    limits.max_vertex_attributes                 = std::min(limits.max_vertex_attributes, 16u);
    limits.max_compute_invocations_per_workgroup = std::min(limits.max_compute_invocations_per_workgroup, 256u);
    limits.max_compute_workgroup_size_x          = std::min(limits.max_compute_workgroup_size_x, 256u);
    limits.max_compute_workgroup_size_y          = std::min(limits.max_compute_workgroup_size_y, 256u);

    // texture row pitch alignment
    properties.texture_row_pitch_alignment = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT; // 256

    // push constant alignment
    // (push constant is not a native concept in D3D12, our current implementation does not have this requirement)
    adapter.properties.min_uniform_buffer_alignment = 32;

    // wave/subgroup properties (requires shader model 6.0+)
    if (shader_model.HighestShaderModel >= D3D_SHADER_MODEL_6_0) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 d3d12_options1 = {};
        HRESULT                           hr             = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &d3d12_options1, sizeof(d3d12_options1));
        if (SUCCEEDED(hr)) {
            properties.subgroup_min_size = d3d12_options1.WaveLaneCountMin;
            properties.subgroup_max_size = d3d12_options1.WaveLaneCountMax;
        }
    }
}

bool api::create_adapter(GPUAdapterProps& adapter, const GPUAdapterDescriptor& desc)
{
    auto rhi = get_rhi();

    // create adapter
    for (UINT adapter_index = 0;; ++adapter_index) {
        IDXGIAdapter1* current_adapter = nullptr;

        // try to get the next adapter
        if (DXGI_ERROR_NOT_FOUND == rhi->factory->EnumAdapters1(adapter_index, &current_adapter))
            break; // no more adapters

        DXGI_ADAPTER_DESC1 adapter_desc;
        current_adapter->GetDesc1(&adapter_desc);

        // ignore Basic Render Driver adapter
        if (adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            current_adapter->Release();
            continue;
        }

        // check if the adapter supports Direct3D 12
        ID3D12Device* current_device = nullptr;
        if (SUCCEEDED(D3D12CreateDevice(current_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&current_device)))) {
            // this adapter works - store it
            current_device->Release();
            rhi->adapter = current_adapter;
            break;
        }

        // This adapter doesn't work, release it
        current_adapter->Release();
    }

    populate_adapter_properties(adapter.limits, adapter.properties);
    return rhi->adapter != nullptr;
}

void api::delete_adapter()
{
    auto rhi = get_rhi();

    if (rhi->adapter) {
        rhi->adapter->Release();
        rhi->adapter = nullptr;
    }
}
