#ifndef LYRA_TESTLIB_HELPER_APP_H
#define LYRA_TESTLIB_HELPER_APP_H

#include "./common.h"
#include "./render.h"

struct TestAppDescriptor
{
    CString       name           = "";
    bool          window         = false;
    uint          width          = 960;
    uint          height         = 480;
    RHIBackend    backend        = RHIBackend::VULKAN;
    RHIFlags      rhi_flags      = RHIFlag::DEBUG | RHIFlag::VALIDATION;
    CompileTarget compile_target = CompileTarget::SPIRV;
    CompileFlags  compile_flags  = CompileFlag::DEBUG;
};

struct TestApp
{
    explicit TestApp(const TestAppDescriptor& desc);

    virtual void render(const GPUSurfaceTexture& texture) = 0;
    virtual void update(const WindowInput& input) {}

    void run();
    void run_with_window();
    void run_without_window();
    void postprocessing(const GPUCommandBuffer& cmd, GPUTextureHandle backbuffer);
    auto get_backbuffer_format() const -> GPUTextureFormat;

    TestAppDescriptor       desc;
    GPUSurface              swp;
    OwnedResource<Window>   win;
    OwnedResource<RHI>      rhi;
    OwnedResource<Compiler> compiler;
    RenderTarget            render_target;
    GPUBindGroupHeap        bheap;
};

#endif // LYRA_TESTLIB_HELPER_APP_H
