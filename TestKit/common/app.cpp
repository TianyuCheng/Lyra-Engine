#include "./app.h"

TestApp::TestApp(const TestAppDescriptor& app_desc) : desc(app_desc)
{
    // initialize window
    if (desc.window) {
        auto desc   = WindowDescriptor{};
        desc.title  = "Lyra Engine :: Test";
        desc.width  = app_desc.width;
        desc.height = app_desc.height;
        win         = Window::init(desc);
    }

    // initialize rhi
    if (true) {
        auto desc    = RHIDescriptor{};
        desc.backend = app_desc.backend;
        desc.flags   = app_desc.rhi_flags;
        if (app_desc.window)
            desc.window = *win;
        rhi = RHI::init(desc);
    }

    // initialize GPU adatper
    auto adapter = execute([&]() {
        auto desc = GPUAdapterDescriptor{};
        return rhi->request_adapter(desc);
    });

    // initialize GPU device
    auto device = execute([&]() {
        auto desc              = GPUDeviceDescriptor{};
        desc.label             = "main_device";
        desc.required_features = app_desc.required_features;
        return adapter.request_device(desc);
    });

    // initialize swapchain
    if (desc.window) {
        auto desc         = GPUSurfaceDescriptor{};
        desc.label        = "main_surface";
        desc.window       = *win;
        desc.present_mode = GPUPresentMode::Fifo;
        swp               = rhi->request_surface(desc);
    }

    // initialize compiler
    compiler = execute([&]() {
        auto desc   = CompilerDescriptor{};
        desc.target = app_desc.compile_target;
        desc.flags  = app_desc.compile_flags;
        return Compiler::init(desc);
    });

    // initialize bind group heap
    bheap = execute([&]() {
        auto desc      = GPUBindGroupHeapDescriptor{};
        desc.page_size = 32;
        return device.create_bind_group_heap(desc);
    });

    // initialize render target
    render_target = RenderTarget::create(get_backbuffer_format(), desc.width, desc.height);
}

void TestApp::run()
{
    if (desc.window) {
        run_with_window();
    } else {
        run_without_window();
    }
}

void TestApp::on_update(const Window& window)
{
    this->update(window.get_input_state());
}

void TestApp::on_render(const Window&)
{
    auto texture = this->swp.get_current_texture();
    if (!texture.suboptimal) {
        render(texture);
        texture.present();
    }
}

void TestApp::on_close(const Window&)
{
    RHI::get_current_device().wait();
}

void TestApp::run_with_window()
{
    win->bind<WindowEvent::UPDATE, &TestApp::on_update>(*this);
    win->bind<WindowEvent::RENDER, &TestApp::on_render>(*this);
    win->bind<WindowEvent::CLOSE, &TestApp::on_close>(*this);
    win->loop();
}

void TestApp::run_without_window()
{
    GPUSurfaceTexture backbuffer;
    backbuffer.texture = render_target.texture.handle;
    backbuffer.view    = render_target.view.handle;

    WindowInput input{}; // dummy window input (since we have no window)

    RHI::new_frame();
    update(input);
    render(backbuffer);
    RHI::end_frame();

    auto& device = RHI::get_current_device();
    device.wait();

    String name = String(desc.name) + ".png";
    render_target.save(name.c_str());
}

void TestApp::postprocessing(const GPUCommandBuffer& cmd, GPUTextureHandle backbuffer)
{
    if (desc.window) {
        cmd.resource_barrier(state_transition(backbuffer, color_attachment_state(), present_src_state()));
    } else {
        cmd.resource_barrier(state_transition(backbuffer, color_attachment_state(), copy_src_state()));
        cmd.copy_texture_to_buffer(render_target.copy_src(), render_target.copy_dst(), render_target.copy_ext());
    }
}

void TestApp::postprocessing_compute(const GPUCommandBuffer& cmd, GPUTextureHandle backbuffer)
{
    if (desc.window) {
        // copy to backbuffer
        GPUTexelCopyTextureInfo src{};
        src.texture = render_target.texture;
        src.aspect  = GPUTextureAspect::COLOR;

        GPUTexelCopyTextureInfo dst{};
        dst.texture = backbuffer;
        dst.aspect  = GPUTextureAspect::COLOR;

        cmd.resource_barrier(state_transition(render_target.texture, unordered_access_state(GPUBarrierSync::COMPUTE), copy_src_state()));
        cmd.resource_barrier(state_transition(backbuffer, undefined_state(), copy_dst_state()));
        cmd.copy_texture_to_texture(src, dst, render_target.copy_ext());
        cmd.resource_barrier(state_transition(backbuffer, copy_dst_state(), present_src_state()));
    } else {
        cmd.resource_barrier(state_transition(render_target.texture, unordered_access_state(GPUBarrierSync::COMPUTE), copy_src_state()));
        cmd.copy_texture_to_buffer(render_target.copy_src(), render_target.copy_dst(), render_target.copy_ext());
    }
}

GPUTextureFormat TestApp::get_backbuffer_format() const
{
    if (desc.window) {
        return swp.get_current_format();
    } else {
        return GPUTextureFormat::RGBA8UNORM;
    }
}
