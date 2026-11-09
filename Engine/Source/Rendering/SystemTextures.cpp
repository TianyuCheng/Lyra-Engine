#include <Lyra/Rendering/SystemTextures.h>
#include <Lyra/Graphics/RHIAPI.h>
#include <Lyra/Graphics/RHIEnums.h>
#include <Lyra/Graphics/RHIDescs.h>
#include <Lyra/Graphics/RHIInits.h>
#include <Lyra/Graphics/RHIUtils.h>
#include <Lyra/Compiler/SLCTypes.h>
#include <Lyra/Utilities/Math.h>
#include <Lyra/Utilities/Logger.h>

using namespace lyra;

// -----------------------------------------------------------------------------
// Slang Compute Shader Sources for Procedural Textures
// -----------------------------------------------------------------------------

static CString k_brdf_lut_shader = R"""(
import lyra;

static const float PI      = 3.14159265358979323846;
static const float TWO_PI  = 6.28318530717958647692;
static const float EPSILON = 1e-6;

float radical_inverse_vdc(uint bits)
{
    bits = reversebits(bits);
    return float(bits) * 2.3283064365386963e-10;
}

float2 hammersley_2d(uint i, uint N)
{
    return float2(float(i) / float(N), radical_inverse_vdc(i));
}

float3 sample_ggx(float2 u, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float phi = TWO_PI * u.x;
    float cos_theta = sqrt(max(0.0, (1.0 - u.y) / (1.0 + (a2 - 1.0) * u.y)));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

    float3 H = float3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float visibility_smith_ggx_correlated(float NoV, float NoL, float a2)
{
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / max(GGXV + GGXL, EPSILON);
}

float2 integrate_brdf(float NoV, float roughness, uint num_samples)
{
    NoV = clamp(NoV, 1e-4, 1.0);
    float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
    float3 N = float3(0.0, 0.0, 1.0);
    float a = roughness * roughness;
    float a2 = a * a;

    float scale = 0.0;
    float bias  = 0.0;

    for (uint i = 0; i < num_samples; ++i) {
        float2 u = hammersley_2d(i, num_samples);
        float3 H = sample_ggx(u, roughness, N);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NoL = max(L.z, 0.0);
        float NoH = max(H.z, 0.0);
        float VoH = max(dot(V, H), 0.0);

        if (NoL > 0.0) {
            float V_pdf = (visibility_smith_ggx_correlated(NoV, NoL, a2) * VoH) / max(NoH, EPSILON);
            float Fc = pow(1.0 - VoH, 5.0);

            scale += (1.0 - Fc) * V_pdf;
            bias  += Fc * V_pdf;
        }
    }

    return float2(scale, bias) * (4.0 / float(num_samples));
}

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_lut;

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 256 || pixel.y >= 256) {
        return;
    }

    float NoV       = max((float(pixel.x) + 0.5) / 256.0, 1e-4);
    float roughness = max((float(pixel.y) + 0.5) / 256.0, 1e-4);

    float2 integrated = integrate_brdf(NoV, roughness, 1024);

    out_lut[pixel] = float4(integrated.x, integrated.y, 0.0, 1.0);
}
)""";

static CString k_skymap_shader = R"""(
import lyra;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_skymap;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 64 || pixel.y >= 64) {
        return;
    }

    float v = (float(pixel.y) + 0.5) / 64.0;

    float3 zenith_color  = float3(0.18, 0.42, 0.85);
    float3 horizon_color = float3(0.70, 0.78, 0.90);
    float3 ground_color  = float3(0.13, 0.13, 0.15);

    float3 color;
    if (v <= 0.5) {
        float h = 1.0 - (v * 2.0);
        float t = pow(1.0 - h, 2.2);
        color = lerp(zenith_color, horizon_color, t);
    } else {
        float g = (v - 0.5) * 2.0;
        float t = min(1.0, g * 1.8);
        color = lerp(horizon_color, ground_color, t);
    }

    out_skymap[pixel] = float4(color, 1.0);
}
)""";

static CString k_checkerboard_shader = R"""(
import lyra;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_checkerboard;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 16 || pixel.y >= 16) {
        return;
    }

    uint tile_size = 4;
    bool is_even = ((pixel.x / tile_size) + (pixel.y / tile_size)) % 2 == 0;

    float4 color_dark  = float4(50.0 / 255.0, 50.0 / 255.0, 50.0 / 255.0, 1.0);
    float4 color_light = float4(70.0 / 255.0, 70.0 / 255.0, 70.0 / 255.0, 1.0);

    out_checkerboard[pixel] = is_even ? color_dark : color_light;
}
)""";

static CString k_ies_profiles_shader = R"""(
import lyra;

static const float PI = 3.14159265358979323846;
static const float HALF_PI = 1.57079632679489661923;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_ies_atlas;

float compute_ies_profile(uint profile, float theta)
{
    float cos_theta = cos(theta);

    switch (profile)
    {
    case 0:
        return 1.0;
    case 1:
        return pow(max(cos_theta, 0.0), 128.0);
    case 2:
        return pow(max(cos_theta, 0.0), 32.0);
    case 3:
        return pow(max(cos_theta, 0.0), 8.0);
    case 4:
        return pow(max(cos_theta, 0.0), 2.5);
    case 5:
        return max(cos_theta, 0.0);
    case 6:
        return max(pow(max(cos_theta, 0.0), 8.0), pow(max(-cos_theta, 0.0), 8.0));
    case 7:
        if (theta > HALF_PI) return 0.0;
        return clamp((sin(theta) / (pow(theta - 0.60, 2.0) + 0.15)) * 0.28, 0.0, 1.0);
    case 8:
        return pow(max(-cos_theta, 0.0), 4.0);
    case 9:
        return pow(max(cos_theta, 0.0), 64.0) * smoothstep(PI * 0.25, 0.0, theta);
    case 10:
        if (theta > HALF_PI) return 0.0;
        return pow(max(cos_theta, 0.0), 1.5) * (1.0 + 0.5 * sin(theta * 2.0));
    case 11:
        return clamp(pow(max(cos_theta, 0.0), 64.0) + exp(-pow((theta - 0.5) / 0.08, 2.0)) * 0.35, 0.0, 1.0);
    case 12:
        return pow(max(cos_theta, 0.0), 1.2);
    case 13:
        return 0.2 + 0.8 * smoothstep(PI * 0.9, PI * 0.2, theta);
    case 14:
        return pow(max(-cos_theta, 0.0), 2.0) * (0.5 + 0.5 * sin(theta));
    case 15:
    default:
        return pow(max(cos_theta, 0.0), 16.0);
    }
}

[shader("compute")]
[numthreads(16, 16, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 256 || pixel.y >= 16) {
        return;
    }

    float u = (float(pixel.x) + 0.5) / 256.0;
    float theta = u * PI;
    uint profile = pixel.y;

    float intensity = compute_ies_profile(profile, theta);

    out_ies_atlas[pixel] = float4(intensity, intensity, intensity, 1.0);
}
)""";

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

    white_texture       = create_solid_texture("system_white");
    white_view          = white_texture.create_view();

    black_texture       = create_solid_texture("system_black");
    black_view          = black_texture.create_view();

    normal_texture      = create_solid_texture("system_flat_normal");
    normal_view         = normal_texture.create_view();

    bump_texture        = create_solid_texture("system_flat_bump");
    bump_view           = bump_texture.create_view();

    default_pbr_texture = create_solid_texture("system_default_pbr");
    default_pbr_view    = default_pbr_texture.create_view();

    magenta_texture     = create_solid_texture("system_magenta_error");
    magenta_view        = magenta_texture.create_view();

    blue_noise_texture  = create_solid_texture("system_blue_noise");
    blue_noise_view     = blue_noise_texture.create_view();

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
        {white_texture,       white_pixel,   0},
        {black_texture,       black_pixel,   0},
        {normal_texture,      normal_pixel,  0},
        {bump_texture,        bump_pixel,    0},
        {default_pbr_texture, pbr_pixel,     0},
        {magenta_texture,     magenta_pixel, 0},
        {blue_noise_texture,  noise_pixel,   0},
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
    CString        shader_source,
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
    desc.path   = module_name;
    desc.source = shader_source;

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

    auto sm_desc  = GPUShaderModuleDescriptor{};
    sm_desc.label = module_name;
    sm_desc.data  = blob->data;
    sm_desc.size  = blob->size;
    auto gpu_module = device.create_shader_module(sm_desc);

    Vector<GPUBindGroupLayoutHandle> blayouts;
    for (auto& bl_desc : reflection->get_bind_group_layouts()) {
        blayouts.push_back(device.create_bind_group_layout(bl_desc));
    }

    auto playout_desc               = GPUPipelineLayoutDescriptor{};
    playout_desc.bind_group_layouts = blayouts;
    playout_desc.immediate_size     = reflection->get_immediate_size();
    auto playout = device.create_pipeline_layout(playout_desc);

    auto cp_desc                = GPUComputePipelineDescriptor{};
    cp_desc.label               = module_name;
    cp_desc.layout              = playout;
    cp_desc.compute.module      = gpu_module;
    cp_desc.compute.entry_point = "csmain";
    auto pipeline = device.create_compute_pipeline(cp_desc);

    auto heap_desc      = GPUBindGroupHeapDescriptor{};
    heap_desc.page_size = 512;
    auto heap = device.create_bind_group_heap(heap_desc);

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
    brdf_lut_texture     = create_storage_tex(BRDF_SIZE, BRDF_SIZE, "system_brdf_lut");
    brdf_lut_view        = brdf_lut_texture.create_view();

    skymap_texture       = create_storage_tex(SKY_WIDTH, SKY_HEIGHT, "system_skymap");
    skymap_view          = skymap_texture.create_view();

    checkerboard_texture = create_storage_tex(CHECKER_SIZE, CHECKER_SIZE, "system_checkerboard");
    checkerboard_view    = checkerboard_texture.create_view();

    ies_atlas_texture    = create_storage_tex(IES_WIDTH, IES_HEIGHT, "system_ies_atlas");
    ies_atlas_view       = ies_atlas_texture.create_view();

    // 2. attempt GPU compute shader execution
    bool brdf_ok    = dispatch_compute_lut(device, compiler, "brdf_lut", k_brdf_lut_shader, brdf_lut_texture, brdf_lut_view, BRDF_SIZE, BRDF_SIZE, 16, 16);
    bool skymap_ok  = dispatch_compute_lut(device, compiler, "skymap", k_skymap_shader, skymap_texture, skymap_view, SKY_WIDTH, SKY_HEIGHT, 8, 8);
    bool checker_ok = dispatch_compute_lut(device, compiler, "checkerboard", k_checkerboard_shader, checkerboard_texture, checkerboard_view, CHECKER_SIZE, CHECKER_SIZE, 8, 8);
    bool ies_ok     = dispatch_compute_lut(device, compiler, "ies_profiles", k_ies_profiles_shader, ies_atlas_texture, ies_atlas_view, IES_WIDTH, IES_HEIGHT, 16, 16);

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
