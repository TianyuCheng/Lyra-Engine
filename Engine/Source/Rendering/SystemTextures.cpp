#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Logger.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Rendering/SystemTextures.h>

using namespace lyra;

// -----------------------------------------------------------------------------
// SystemTextures Implementation
// -----------------------------------------------------------------------------

SystemTextures::SystemTextures() = default;

SystemTextures::~SystemTextures()
{
    shutdown();
}

bool SystemTextures::init(GPUDevice device, Compiler compiler, AssetServer* ams)
{
    if (initialized) {
        return true;
    }

    this->device = device;

    // 1. initialize standard 1x1 solid fallback textures (fast single-buffer staging upload)
    init_solid_textures(device);

    // 2. initialize procedural textures on GPU via compute shaders
    init_procedural_textures(device, compiler, ams);

    initialized = true;
    return true;
}

void SystemTextures::shutdown()
{
    if (!initialized) {
        return;
    }

    // ensure all GPU work referencing these textures has completed before deletion
    device.wait();

    white_view.destroy();
    white_texture.destroy();

    black_view.destroy();
    black_texture.destroy();

    normal_view.destroy();
    normal_texture.destroy();

    bump_view.destroy();
    bump_texture.destroy();

    default_pbr_view.destroy();
    default_pbr_texture.destroy();

    magenta_view.destroy();
    magenta_texture.destroy();

    checkerboard_view.destroy();
    checkerboard_texture.destroy();

    skymap_view.destroy();
    skymap_texture.destroy();

    brdf_lut_view.destroy();
    brdf_lut_texture.destroy();

    ies_atlas_view.destroy();
    ies_atlas_texture.destroy();

    blue_noise_view.destroy();
    blue_noise_texture.destroy();

    initialized = false;
}

void SystemTextures::init_solid_textures(GPUDevice device)
{
    auto create_solid_texture = [&](CString label) -> GPUTexture {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = label;
        desc.size.width      = 1;
        desc.size.height     = 1;
        desc.size.depth      = 1;
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA8UNORM;
        desc.usage           = GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        desc.sample_count    = 1;
        return device.create_texture(desc);
    };

    white_texture = create_solid_texture("system_white");
    white_view    = white_texture.create_view();

    black_texture = create_solid_texture("system_black");
    black_view    = black_texture.create_view();

    normal_texture = create_solid_texture("system_flat_normal");
    normal_view    = normal_texture.create_view();

    bump_texture = create_solid_texture("system_flat_bump");
    bump_view    = bump_texture.create_view();

    default_pbr_texture = create_solid_texture("system_default_pbr");
    default_pbr_view    = default_pbr_texture.create_view();

    magenta_texture = create_solid_texture("system_magenta_error");
    magenta_view    = magenta_texture.create_view();

    blue_noise_texture = create_solid_texture("system_blue_noise");
    blue_noise_view    = blue_noise_texture.create_view();

    // 1x1 solid pixel values
    const uint32_t white_pixel   = 0xFFFFFFFF; // white
    const uint32_t black_pixel   = 0xFF000000; // black
    const uint32_t normal_pixel  = 0xFFFF8080; // flat tangent normal: (128, 128, 255, 255)
    const uint32_t bump_pixel    = 0xFF808080; // neutral flat bump: 0.5 height
    const uint32_t pbr_pixel     = 0xFF00FF00; // default PBR: roughness = 1.0 (G), metallic = 0.0 (B)
    const uint32_t magenta_pixel = 0xFFFF00FF; // magenta / purple for unsupported materials
    const uint32_t noise_pixel   = 0xFF808080; // neutral blue noise placeholder

    struct SolidJob
    {
        GPUTexture texture;
        uint32_t   pixel;
        uint32_t   offset;
    };

    Vector<SolidJob> jobs = {
        {white_texture, white_pixel, 0},
        {black_texture, black_pixel, 0},
        {normal_texture, normal_pixel, 0},
        {bump_texture, bump_pixel, 0},
        {default_pbr_texture, pbr_pixel, 0},
        {magenta_texture, magenta_pixel, 0},
        {blue_noise_texture, noise_pixel, 0},
    };

    // align staging offsets to 256 bytes for multi-hardware compliance
    uint32_t total_size = 0;
    for (auto& job : jobs) {
        job.offset = total_size;
        total_size += 256;
    }

    auto staging_desc               = GPUBufferDescriptor{};
    staging_desc.label              = "system_textures_staging_buffer";
    staging_desc.size               = total_size;
    staging_desc.usage              = GPUBufferUsage::MAP_WRITE | GPUBufferUsage::COPY_SRC;
    staging_desc.mapped_at_creation = true;

    auto staging_buffer = device.create_buffer(staging_desc);
    auto mapped_range   = staging_buffer.get_mapped_range();
    auto dst_ptr        = static_cast<uint8_t*>(mapped_range.data);

    for (const auto& job : jobs) {
        std::memcpy(dst_ptr + job.offset, &job.pixel, sizeof(uint32_t));
    }
    staging_buffer.unmap();

    auto cmd_desc  = GPUCommandBufferDescriptor{};
    cmd_desc.label = "system_textures_solid_init_cmd";
    cmd_desc.queue = GPUQueueType::DEFAULT;
    auto cmdbuf    = device.create_command_buffer(cmd_desc);

    for (const auto& job : jobs) {
        cmdbuf.resource_barrier(state_transition(job.texture, undefined_texture_state(), copy_dst_texture_state()));

        GPUTexelCopyBufferInfo src{};
        src.buffer         = staging_buffer;
        src.offset         = job.offset;
        src.bytes_per_row  = 256;
        src.rows_per_image = 1;

        GPUTexelCopyTextureInfo dst{};
        dst.texture   = job.texture;
        dst.aspect    = GPUTextureAspect::COLOR;
        dst.mip_level = 0;
        dst.origin    = {0, 0, 0};

        GPUExtent3D extent{1, 1, 1};
        cmdbuf.copy_buffer_to_texture(src, dst, extent);

        cmdbuf.resource_barrier(state_transition(job.texture, copy_dst_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
    }

    cmdbuf.submit();
    device.wait();

    staging_buffer.destroy();
}

bool SystemTextures::dispatch_compute_lut(
    GPUDevice      device,
    Compiler       compiler,
    CString        module_name,
    CString        shader_path,
    GPUTexture     texture,
    GPUTextureView view,
    uint           width,
    uint           height,
    uint           group_size_x,
    uint           group_size_y)
{
    if (!compiler.handle.valid()) {
        return false;
    }

    auto desc   = CompileDescriptor{};
    desc.module = module_name;
    desc.path   = shader_path;
    desc.source = nullptr;

    Own<ShaderModule> shader_module;
    try {
        shader_module = compiler.compile(desc);
    } catch (const std::exception& e) {
        spdlog::warn("failed to compile compute shader for {}: {}", module_name, e.what());
        return false;
    }

    if (!shader_module) {
        return false;
    }

    auto reflection = compiler.reflect({{*shader_module, "csmain"}});
    if (!reflection) {
        return false;
    }

    auto blob = shader_module->get_shader_blob("csmain");
    if (!blob) {
        return false;
    }

    auto sm_desc    = GPUShaderModuleDescriptor{};
    sm_desc.label   = module_name;
    sm_desc.data    = blob->data;
    sm_desc.size    = blob->size;
    auto gpu_module = device.create_shader_module(sm_desc);

    Vector<GPUBindGroupLayoutHandle> blayouts;
    for (auto& bl_desc : reflection->get_bind_group_layouts()) {
        blayouts.push_back(device.create_bind_group_layout(bl_desc));
    }

    auto playout_desc               = GPUPipelineLayoutDescriptor{};
    playout_desc.bind_group_layouts = blayouts;
    playout_desc.immediate_size     = reflection->get_immediate_size();
    auto playout                    = device.create_pipeline_layout(playout_desc);

    auto cp_desc                = GPUComputePipelineDescriptor{};
    cp_desc.label               = module_name;
    cp_desc.layout              = playout;
    cp_desc.compute.module      = gpu_module;
    cp_desc.compute.entry_point = "csmain";
    auto pipeline               = device.create_compute_pipeline(cp_desc);

    auto heap_desc      = GPUBindGroupHeapDescriptor{};
    heap_desc.page_size = 512;
    auto heap           = device.create_bind_group_heap(heap_desc);

    Array<GPUBindGroupEntry, 1> entries{};
    entries[0].type    = GPUResourceType::STORAGE_TEXTURE;
    entries[0].binding = 0;
    entries[0].texture = view.handle;

    auto bg_desc    = GPUBindGroupDescriptor{};
    bg_desc.heap    = heap;
    bg_desc.layout  = blayouts.at(0);
    bg_desc.entries = entries;
    auto bind_group = device.create_bind_group(bg_desc);

    auto cmd_desc  = GPUCommandBufferDescriptor{};
    cmd_desc.label = module_name;
    cmd_desc.queue = GPUQueueType::DEFAULT;
    auto cmdbuf    = device.create_command_buffer(cmd_desc);

    cmdbuf.resource_barrier(state_transition(texture, undefined_texture_state(), unordered_access_texture_state(GPUBarrierSync::COMPUTE)));
    cmdbuf.set_pipeline(pipeline);
    cmdbuf.set_bind_group(0, bind_group);
    cmdbuf.dispatch_workgroups((width + group_size_x - 1) / group_size_x, (height + group_size_y - 1) / group_size_y, 1);
    cmdbuf.resource_barrier(state_transition(texture, unordered_access_texture_state(GPUBarrierSync::COMPUTE), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));

    cmdbuf.submit();
    device.wait();

    heap.destroy();
    pipeline.destroy();
    playout.destroy();
    for (auto& bl : blayouts) {
        RHI::api()->delete_bind_group_layout(bl);
    }
    gpu_module.destroy();

    return true;
}

void SystemTextures::init_procedural_textures(GPUDevice device, Compiler compiler, AssetServer* ams)
{
    constexpr uint32_t BRDF_SIZE    = 256;
    constexpr uint32_t SKY_WIDTH    = 64;
    constexpr uint32_t SKY_HEIGHT   = 64;
    constexpr uint32_t CHECKER_SIZE = 16;
    constexpr uint32_t IES_WIDTH    = 256;
    constexpr uint32_t IES_HEIGHT   = 16;

    auto create_storage_tex = [&](uint32_t w, uint32_t h, CString label) -> GPUTexture {
        auto desc            = GPUTextureDescriptor{};
        desc.label           = label;
        desc.size.width      = w;
        desc.size.height     = h;
        desc.size.depth      = 1;
        desc.dimension       = GPUTextureDimension::x2D;
        desc.format          = GPUTextureFormat::RGBA8UNORM;
        desc.usage           = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::TEXTURE_BINDING | GPUTextureUsage::COPY_DST;
        desc.mip_level_count = 1;
        desc.sample_count    = 1;
        return device.create_texture(desc);
    };

    // 1. allocate textures with storage binding support
    brdf_lut_texture = create_storage_tex(BRDF_SIZE, BRDF_SIZE, "system_brdf_lut");
    brdf_lut_view    = brdf_lut_texture.create_view();

    skymap_texture = create_storage_tex(SKY_WIDTH, SKY_HEIGHT, "system_skymap");
    skymap_view    = skymap_texture.create_view();

    checkerboard_texture = create_storage_tex(CHECKER_SIZE, CHECKER_SIZE, "system_checkerboard");
    checkerboard_view    = checkerboard_texture.create_view();

    ies_atlas_texture = create_storage_tex(IES_WIDTH, IES_HEIGHT, "system_ies_atlas");
    ies_atlas_view    = ies_atlas_texture.create_view();

    // 2. attempt GPU compute shader execution using shader library in Engine/Assets/Shaders/LUT/
    bool brdf_ok    = dispatch_compute_lut(device, compiler, "LUT.BRDFLut", "LUT/BRDFLut.slang", brdf_lut_texture, brdf_lut_view, BRDF_SIZE, BRDF_SIZE, 16, 16);
    bool skymap_ok  = dispatch_compute_lut(device, compiler, "LUT.Skymap", "LUT/Skymap.slang", skymap_texture, skymap_view, SKY_WIDTH, SKY_HEIGHT, 8, 8);
    bool checker_ok = dispatch_compute_lut(device, compiler, "LUT.Checkerboard", "LUT/Checkerboard.slang", checkerboard_texture, checkerboard_view, CHECKER_SIZE, CHECKER_SIZE, 8, 8);
    bool ies_ok     = dispatch_compute_lut(device, compiler, "LUT.IESProfiles", "LUT/IESProfiles.slang", ies_atlas_texture, ies_atlas_view, IES_WIDTH, IES_HEIGHT, 16, 16);

    // 3. fast fallback if compiler is unavailable (e.g., headless unit testing)
    if (!brdf_ok || !skymap_ok || !checker_ok || !ies_ok) {
        auto cmd_desc  = GPUCommandBufferDescriptor{};
        cmd_desc.label = "system_procedural_fallback_transition";
        cmd_desc.queue = GPUQueueType::DEFAULT;
        auto cmdbuf    = device.create_command_buffer(cmd_desc);

        if (!brdf_ok) {
            cmdbuf.resource_barrier(state_transition(brdf_lut_texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        }
        if (!skymap_ok) {
            cmdbuf.resource_barrier(state_transition(skymap_texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        }
        if (!checker_ok) {
            cmdbuf.resource_barrier(state_transition(checkerboard_texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        }
        if (!ies_ok) {
            cmdbuf.resource_barrier(state_transition(ies_atlas_texture, undefined_texture_state(), shader_resource_texture_state(GPUBarrierSync::ALL_SHADING)));
        }

        cmdbuf.submit();
        device.wait();
    }
}
