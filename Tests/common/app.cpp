#include "./app.h"
#include "./context.h"
#include <fstream>
#include <spdlog/spdlog.h>
#include <doctest/doctest.h>

using namespace lyra;

static auto resolve_output_path(const String& filename) -> Path
{
    const char* runs_dir_env = std::getenv("LYRA_TESTKIT_RUNS_DIR");
    if (!runs_dir_env || runs_dir_env[0] == '\0') {
        return Path(filename);
    }

    const auto opts = doctest::getContextOptions();
    if (opts && opts->currentTest && opts->currentTest->m_name) {
        StringView test_name(opts->currentTest->m_name);
        auto       last_sep = test_name.rfind("::");
        if (last_sep != StringView::npos) {
            StringView      subfolder = test_name.substr(last_sep + 2);
            Path            dir       = Path(runs_dir_env) / String(subfolder);
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            return dir / filename;
        }
    }
    return Path(runs_dir_env) / filename;
}

TestApp::TestApp(const TestAppDescriptor& app_desc) : desc(app_desc)
{
    // find engine repository root
    std::filesystem::path repo_root;
    if (const char* env_root = std::getenv("LYRA_REPO_ROOT")) {
        repo_root = env_root;
    }
    if (repo_root.empty() || !std::filesystem::exists(repo_root / "Engine" / "Assets")) {
        repo_root = std::filesystem::current_path();
        while (!repo_root.empty() && !std::filesystem::exists(repo_root / "Engine" / "Assets")) {
            auto parent = repo_root.parent_path();
            if (parent == repo_root) break;
            repo_root = parent;
        }
    }

    static String engine_assets_str  = (repo_root / "Engine" / "Assets").lexically_normal().string();
    static String engine_shaders_str = (repo_root / "Engine" / "Assets" / "Shaders").lexically_normal().string();

    if (desc.window) {
        auto win_desc   = WindowDescriptor{};
        win_desc.title  = "Lyra Engine :: Test";
        win_desc.width  = app_desc.width;
        win_desc.height = app_desc.height;
        win             = Window::init(win_desc);

        auto rhi_desc    = RHIDescriptor{};
        rhi_desc.backend = app_desc.backend;
        rhi_desc.flags   = app_desc.rhi_flags;
        rhi_desc.window  = *win;
        rhi              = RHI::init(rhi_desc);

        auto adapter_desc = GPUAdapterDescriptor{};
        adapter           = rhi->request_adapter(adapter_desc);

        for (auto feature : app_desc.features) {
            if (!adapter.has_feature(feature)) {
                skipped     = true;
                skip_reason = String("Missing requested feature: ") + to_string(feature);
                spdlog::warn("Test '{}' skipped: {}", desc.name, skip_reason);
                return;
            }
        }

        auto dev_desc     = GPUDeviceDescriptor{};
        dev_desc.label    = "main_device";
        dev_desc.features = app_desc.features;
        device            = adapter.request_device(dev_desc);

        auto srf_desc         = GPUSurfaceDescriptor{};
        srf_desc.label        = "main_surface";
        srf_desc.window       = *win;
        srf_desc.present_mode = GPUPresentMode::Fifo;
        swp                   = rhi->request_surface(srf_desc);

        owned_loader = make_own<FileLoader>(FSLoader::NATIVE);
        owned_loader->mount("sys://", engine_assets_str.c_str(), 0);
        loader = owned_loader.get();
    } else {
        auto& ctx = TestContext::get();
        if (!ctx.acquire(app_desc.backend, app_desc.rhi_flags, app_desc.features)) {
            skipped = true;
            if (ctx.feature_unsupported) {
                skip_reason = String("Missing requested feature: ") + ctx.missing_feature_name;
            } else {
                skip_reason = "Failed to initialize RHI backend";
            }
            spdlog::warn("Test '{}' skipped: {}", desc.name, skip_reason);
            return;
        }

        adapter = ctx.adapter;
        device  = ctx.device;
        loader  = ctx.loader.get();
    }

    // initialize compiler
    compiler = execute([&]() {
        static CString includes[] = {engine_shaders_str.c_str()};

        auto cdesc      = CompilerDescriptor{};
        cdesc.target    = resolve_compile_target(app_desc.compile_target, app_desc.backend);
        cdesc.flags     = app_desc.compile_flags;
        cdesc.includes  = includes;
        cdesc.fs_loader = loader;
        return Compiler::init(cdesc);
    });

    // initialize bind group heap
    bheap = execute([&]() {
        auto bdesc      = GPUBindGroupHeapDescriptor{};
        bdesc.page_size = 32;
        return device.create_bind_group_heap(bdesc);
    });

    // initialize render target
    render_target = RenderTarget::create(get_backbuffer_format(), desc.width, desc.height);
}

TestApp::~TestApp()
{
    if (render_target.texture.valid() || render_target.buffer.valid()) {
        render_target.destroy();
    }
    if (bheap.valid()) {
        bheap.destroy();
        bheap.handle.reset();
    }
    compiler.reset();
}

void TestApp::run()
{
    if (skipped) {
        String        filename = String(desc.name) + ".skip";
        Path          out_path = resolve_output_path(filename);
        std::ofstream out(out_path);
        out << skip_reason << "\n";
        out.close();
        return;
    }

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

    auto& current_device = RHI::get_current_device();
    current_device.wait();

    String name     = String(desc.name) + ".png";
    Path   out_path = resolve_output_path(name);
    render_target.save(out_path.string().c_str());
}

void TestApp::postprocessing(const GPUCommandBuffer& cmd, GPUTextureHandle backbuffer)
{
    if (desc.window) {
        cmd.resource_barrier(state_transition(backbuffer, color_attachment_texture_state(), present_src_texture_state()));
    } else {
        cmd.resource_barrier(state_transition(backbuffer, color_attachment_texture_state(), copy_src_texture_state()));
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

        cmd.resource_barrier(state_transition(render_target.texture, unordered_access_texture_state(GPUBarrierSync::COMPUTE), copy_src_texture_state()));
        cmd.resource_barrier(state_transition(backbuffer, undefined_texture_state(), copy_dst_texture_state()));
        cmd.copy_texture_to_texture(src, dst, render_target.copy_ext());
        cmd.resource_barrier(state_transition(backbuffer, copy_dst_texture_state(), present_src_texture_state()));
    } else {
        cmd.resource_barrier(state_transition(render_target.texture, unordered_access_texture_state(GPUBarrierSync::COMPUTE), copy_src_texture_state()));
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
