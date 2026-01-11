#include "helper.h"

CString ray_tracing_program = R"""(
import lyra;

struct Camera
{
    float4x4 proj;
    float4x4 view;
    float4x4 view_inv;
    float4x4 proj_inv;
};

struct Params
{
    ConstantBuffer<Camera>          camera;
    RaytracingAccelerationStructure tlas;
    RWTexture2D<float4>             output;
};

ParameterBlock<Params> params;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 idx = dispatch_id.xy;
    uint2 dim;
    params.output.GetDimensions(dim.x, dim.y);

    if (idx.x >= dim.x || idx.y >= dim.y) return;

    float2 uv = (float2(idx) + 0.5) / float2(dim);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 target = mul(float4(ndc, 1.0, 1.0), params.camera.proj_inv);
    float3 direction = mul(float4(normalize(target.xyz / target.w), 0.0), params.camera.view_inv).xyz;
    float3 origin = mul(float4(0.0, 0.0, 0.0, 1.0), params.camera.view_inv).xyz;

    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    RayQuery<RAY_FLAG_NONE> q;
    q.TraceRayInline(params.tlas, RAY_FLAG_NONE, 0xFF, ray);
    q.Proceed();

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
         float2 bary = q.CommittedTriangleBarycentrics();
         float3 color = float3(1.0 - bary.x - bary.y, bary.x, bary.y);
         params.output[idx] = float4(color, 1.0);
    }
    else
    {
        params.output[idx] = float4(0.0, 0.0, 0.2, 1.0);
    }
}
)""";

struct CameraUniform
{
    glm::mat4 proj;
    glm::mat4 view;
    glm::mat4 view_inv;
    glm::mat4 proj_inv;
};

struct RayTracingApp : public TestApp
{
    Uniform               uniform;
    Geometry              geometry;
    GPUBlas               blas;
    GPUTlas               tlas;
    GPUBuffer             scratch;
    GPUTexture            output_tex;
    GPUTextureView        output_view;
    SimpleComputePipeline pipeline;
    GPUBindGroup          bind_group;

    explicit RayTracingApp(const TestAppDescriptor& desc) : TestApp(desc)
    {
        setup_buffers();
        setup_as();
        setup_pipeline();
        setup_output();
        build_as();
    }

    void setup_buffers()
    {
        auto& device = RHI::get_current_device();

        uint  width  = desc.width;
        uint  height = desc.height;
        float fovy   = 1.05f;
        float aspect = float(width) / float(height);

        geometry = Geometry::create_cube();

        uniform.ubuffer = execute([&]() {
            auto desc               = GPUBufferDescriptor{};
            desc.label              = "camera";
            desc.size               = sizeof(CameraUniform);
            desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
            desc.mapped_at_creation = true;
            return device.create_buffer(desc);
        });

        Camera camera;
        camera.proj = glm::perspective(fovy, aspect, 0.1f, 100.0f);
        camera.view = glm::lookAt(glm::vec3(0.0, 0.0, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        auto mapped           = uniform.ubuffer.get_mapped_range<CameraUniform>();
        mapped.at(0).proj     = camera.proj;
        mapped.at(0).view     = camera.view;
        mapped.at(0).proj_inv = glm::inverse(camera.proj);
        mapped.at(0).view_inv = glm::inverse(camera.view);
    }

    void setup_as()
    {
        auto& device = RHI::get_current_device();

        // BLAS
        blas = execute([&]() {
            GPUBlasDescriptor desc = {};
            desc.label             = "cube_blas";
            desc.flags             = GPUBVHFlag::PREFER_FAST_TRACE;

            GPUBlasGeometrySizeDescriptor size = {};
            size.type                          = GPUBlasType::TRIANGLE;
            size.triangles.flags               = GPUBVHGeometryFlag::BVH_OPAQUE;
            size.triangles.vertex_format       = GPUVertexFormat::FLOAT32x3;
            size.triangles.vertex_count        = 24;
            size.triangles.index_count         = 36;
            size.triangles.index_format        = GPUIndexFormat::UINT32;

            return device.create_blas(desc, {size});
        });

        // TLAS
        tlas = execute([&]() {
            GPUTlasDescriptor desc = {};
            desc.label             = "scene_tlas";
            desc.flags             = GPUBVHFlag::PREFER_FAST_TRACE;
            desc.max_instances     = 1;
            return device.create_tlas(desc);
        });

        // scratch buffer
        scratch = execute([&]() {
            GPUBufferDescriptor desc = {};
            desc.label               = "as_scratch";
            desc.size                = 1024 * 1024 * 4; // 4MB
            desc.usage               = GPUBufferUsage::STORAGE;
            return device.create_buffer(desc);
        });
    }

    void setup_pipeline()
    {
        auto& device = RHI::get_current_device();

        auto module = execute([&]() {
            auto desc   = CompileDescriptor{};
            desc.module = "raytracing";
            desc.path   = "raytracing.slang";
            desc.source = ray_tracing_program;
            return compiler->compile(desc);
        });

        auto reflection = compiler->reflect({{*module, "csmain"}});

        pipeline.init_cshader(device, module.get(), "csmain");
        pipeline.init_playout(device, reflection.get());
        pipeline.init_pipeline(device, reflection.get());
    }

    void setup_output()
    {
        auto& device = RHI::get_current_device();

        GPUTextureDescriptor desc = {};
        desc.label                = "output";
        desc.format               = GPUTextureFormat::RGBA8UNORM;
        desc.size                 = {this->desc.width, this->desc.height, 1};
        desc.array_layers         = 1;
        desc.mip_level_count      = 1;
        desc.sample_count         = 1;
        desc.dimension            = GPUTextureDimension::x2D;
        desc.usage                = GPUTextureUsage::STORAGE_BINDING | GPUTextureUsage::COPY_SRC | GPUTextureUsage::TEXTURE_BINDING;

        output_tex  = device.create_texture(desc);
        output_view = output_tex.create_view();

        // create bind group
        Array<GPUBindGroupEntry, 3> entries = {};

        entries[0].type          = GPUResourceType::BUFFER;
        entries[0].binding       = 0;
        entries[0].buffer.buffer = uniform.ubuffer;
        entries[0].buffer.offset = 0;
        entries[0].buffer.size   = sizeof(CameraUniform);

        entries[1].type    = GPUResourceType::ACCELERATION_STRUCTURE;
        entries[1].binding = 1;
        entries[1].tlas    = tlas;

        entries[2].type    = GPUResourceType::TEXTURE;
        entries[2].binding = 2;
        entries[2].texture = output_view;

        GPUBindGroupDescriptor bg_desc = {};
        bg_desc.layout                 = pipeline.blayouts.at(0);
        bg_desc.heap                   = bheap;
        bg_desc.entries                = entries;

        bind_group = device.create_bind_group(bg_desc);
    }

    void build_as()
    {
        auto& device = RHI::get_current_device();

        auto cmd = execute([&]() {
            GPUCommandBufferDescriptor desc = {};
            desc.queue                      = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        // build BLAS
        GPUBlasTriangleGeometry tri = {};
        tri.size.flags              = GPUBVHGeometryFlag::BVH_OPAQUE;
        tri.size.vertex_format      = GPUVertexFormat::FLOAT32x3;
        tri.size.vertex_count       = 24;
        tri.size.index_count        = 36;
        tri.size.index_format       = GPUIndexFormat::UINT32;
        tri.vertex_buffer           = geometry.vbuffer;
        tri.vertex_stride           = sizeof(Vertex);
        tri.first_vertex            = 0;
        tri.index_buffer            = geometry.ibuffer;
        tri.first_index             = 0;

        Vector<GPUBlasTriangleGeometry> tris;
        tris.push_back(tri);

        GPUBlasBuildEntry blas_entry    = {};
        blas_entry.blas                 = blas;
        blas_entry.geometries.type      = GPUBlasType::TRIANGLE;
        blas_entry.geometries.triangles = tris;

        cmd.build_blases(scratch, blas_entry);

        // barrier for BLAS build completion
        GPUBufferBarrier barrier = {};
        barrier.buffer           = scratch;
        barrier.src_sync         = GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD;
        barrier.dst_sync         = GPUBarrierSync::ACCELERATION_STRUCTURE_BUILD;
        barrier.src_access       = GPUBarrierAccess::ACCELERATION_STRUCTURE_WRITE;
        barrier.dst_access       = GPUBarrierAccess::ACCELERATION_STRUCTURE_READ;
        cmd.resource_barrier(barrier);

        // build TLAS
        GPUTlasInstance instance = {};
        // Identity matrix 3x4
        std::memset(instance.transform, 0, sizeof(instance.transform));
        instance.transform[0][0] = 1.0f;
        instance.transform[1][1] = 1.0f;
        instance.transform[2][2] = 1.0f;
        instance.custom_data     = 0;
        instance.mask            = 0xFF;
        instance.blas            = blas;

        Vector<GPUTlasInstance> instances;
        instances.push_back(instance);

        GPUTlasBuildEntry tlas_entry = {};
        tlas_entry.tlas              = tlas;
        tlas_entry.instances         = instances;

        cmd.build_tlases(scratch, tlas_entry);
        cmd.submit();
        device.wait();
    }

    void render(const GPUSurfaceTexture& backbuffer) override
    {
        auto& device = RHI::get_current_device();
        auto  cmd    = execute([&]() {
            GPUCommandBufferDescriptor desc = {};
            desc.queue                      = GPUQueueType::DEFAULT;
            return device.create_command_buffer(desc);
        });

        // dispatch Compute
        cmd.set_pipeline(pipeline.pipeline);
        cmd.set_bind_group(0, bind_group);
        cmd.dispatch_workgroups((desc.width + 7) / 8, (desc.height + 7) / 8, 1);

        // copy to backbuffer
        GPUTexelCopyTextureInfo src = {};
        src.texture                 = output_tex;
        src.aspect                  = GPUTextureAspect::COLOR;

        GPUTexelCopyTextureInfo dst = {};
        dst.texture                 = backbuffer.texture;
        dst.aspect                  = GPUTextureAspect::COLOR;

        GPUExtent3D extent = {desc.width, desc.height, 1};

        cmd.resource_barrier(state_transition(output_tex, undefined_state(), copy_src_state()));
        cmd.resource_barrier(state_transition(backbuffer.texture, undefined_state(), copy_dst_state()));
        cmd.copy_texture_to_texture(src, dst, extent);
        cmd.resource_barrier(state_transition(backbuffer.texture, copy_dst_state(), present_src_state()));
        cmd.submit();
    }
};

#ifdef LYRA_VULKAN_SUPPORT
TEST_CASE("rhi::vulkan::ray_tracing" * doctest::description("Basic ray tracing using RayQuery."))
{
    TestAppDescriptor desc{};
    desc.name              = "vulkan";
    desc.window            = true;
    desc.backend           = RHIBackend::VULKAN;
    desc.width             = 640;
    desc.height            = 480;
    desc.rhi_flags         = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target    = CompileTarget::SPIRV;
    desc.compile_flags     = CompileFlag::DEBUG | CompileFlag::REFLECT;
    desc.required_features = {GPUFeatureName::RAYTRACING};
    RayTracingApp(desc).run();
}
#endif

#ifdef WIN32
TEST_CASE("rhi::d3d12::ray_tracing" * doctest::description("Basic ray tracing using RayQuery."))
{
    TestAppDescriptor desc{};
    desc.name              = "d3d12";
    desc.window            = false;
    desc.backend           = RHIBackend::D3D12;
    desc.width             = 640;
    desc.height            = 480;
    desc.rhi_flags         = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target    = CompileTarget::DXIL;
    desc.compile_flags     = CompileFlag::DEBUG | CompileFlag::REFLECT;
    desc.required_features = {GPUFeatureName::RAYTRACING};
    RayTracingApp(desc).run();
}
#endif

#ifdef __APPLE__
TEST_CASE("rhi::metal::ray_tracing" * doctest::description("Basic ray tracing using RayQuery."))
{
    TestAppDescriptor desc{};
    desc.name              = "metal";
    desc.window            = false;
    desc.backend           = RHIBackend::METAL;
    desc.width             = 640;
    desc.height            = 480;
    desc.rhi_flags         = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    desc.compile_target    = CompileTarget::MSL;
    desc.compile_flags     = CompileFlag::DEBUG;
    desc.required_features = {GPUFeatureName::RAYTRACING};
    RayTracingApp(desc).run();
}
#endif
