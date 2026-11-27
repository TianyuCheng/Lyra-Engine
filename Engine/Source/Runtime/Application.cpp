#include <Lyra/Runtime/Application.h>
#include <Lyra/JobSystem/JobScheduler.h>

using namespace lyra;

#pragma region AppDescriptor
AppDescriptor& AppDescriptor::with_title(CString title)
{
    wsi.title = title;
    return *this;
}

AppDescriptor& AppDescriptor::with_fullscreen(bool enable)
{
    wsi.flags |= WindowFlag::FULLSCREEN;
    return *this;
}

AppDescriptor& AppDescriptor::with_window_maximized()
{
    wsi.flags |= WindowFlag::MAXIMIZED;
    return *this;
}

AppDescriptor& AppDescriptor::with_window_extent(uint width, uint height)
{
    wsi.width  = width;
    wsi.height = height;
    return *this;
}

AppDescriptor& AppDescriptor::with_graphics_backend(RHIBackend backend)
{
    rhi.backend = backend;
    slc.target  = resolve_compile_target(CompileTarget::AUTO, backend);
    slc.flags |= CompileFlag::REFLECT; // reflect is always on
    return *this;
}

AppDescriptor& AppDescriptor::with_graphics_backend(StringView custom_backend)
{
    rhi.backend        = RHIBackend::CUSTOM;
    rhi.custom_backend = custom_backend;
    slc.target         = CompileTarget::SPIRV;
    slc.flags |= CompileFlag::REFLECT;
    return *this;
}

AppDescriptor& AppDescriptor::with_graphics_validation(bool debug, bool validation)
{
    slc.flags.set(CompileFlag::DEBUG, debug);
    rhi.flags.set(RHIFlag::DEBUG, debug);
    rhi.flags.set(RHIFlag::VALIDATION, validation);
    return *this;
}

AppDescriptor& AppDescriptor::with_frames_in_flight(uint frames_in_flight)
{
    rhi.frames = frames_in_flight;
    return *this;
}

AppDescriptor& AppDescriptor::with_workers(uint workers)
{
    this->jobs.max_workers = workers;
    return *this;
}

AppDescriptor& AppDescriptor::with_filesystem_backend(FSLoader backend)
{
    this->vfs.backend = backend;
    return *this;
}

AppDescriptor& AppDescriptor::with_mount(CString scheme, const Path& physical, uint priority)
{
    this->vfs.mounts.push_back(AppMountPoint{scheme ? scheme : "", physical, priority});
    return *this;
}

AppDescriptor& AppDescriptor::with_file_loader(FileLoader* loader)
{
    this->slc.fs_loader = loader;
    return *this;
}

AppDescriptor& AppDescriptor::with_shader_includes(const Vector<CString>& includes)
{
    this->slc.includes = includes;
    return *this;
}
#pragma endregion AppDescriptor

#pragma region Application
/**
 * @brief Initialize the application.
 */
Application::Application(const AppDescriptor& descriptor)
    : descriptor(descriptor)
{
    init_logger();
    init_filesystem();
    init_job_system();
    init_window();
    init_graphics();
    init_compiler();
    bind_events();

    // adding commonly used components into services
    context.services.add<Application*>(this);
    if (auto fl = get_file_loader()) {
        context.services.add<FileLoader*>(fl);
    }
    context.services.add<Window*>(wsi.get());
    context.services.add<Compiler*>(slc.get());
    context.services.add<GPUAdapter*>(&adapter);
    context.services.add<GPUDevice*>(&device);
    context.services.add<GPUSurface*>(&surface);
}

/**
 * @brief Clean up application resources.
 */
Application::~Application()
{
    // wait for graphics device idle
    rhi->wait();

    // shutdown job scheduler
    JobScheduler::shutdown();

    // reset all owned resources
    slc.reset();
    rhi.reset();
    wsi.reset();
    if (loader) {
        loader->destroy();
        loader.reset();
    }
}

/**
 * @brief Initialize the virtual file system loader.
 */
void Application::init_filesystem()
{
    if (!descriptor.vfs.mounts.empty()) {
        loader = make_own<FileLoader>(descriptor.vfs.backend);
        for (const auto& m : descriptor.vfs.mounts) {
            auto res = loader->mount(m.scheme.c_str(), m.physical, m.priority);
            if (!res) {
                spdlog::warn("failed to mount {} -> {}: {}", m.scheme, m.physical.generic_string(), res.error());
            }
        }
    }
}

/**
 * @brief Initialize the default logger.
 */
void Application::init_logger()
{
    create_default_logger();
}

/**
 * @brief Initialize the window system.
 */
void Application::init_window()
{
    // initialize WSI (window system integration)
    auto win = Window::init(descriptor.wsi);
    if (!win) {
        spdlog::critical("Failed to initialize window: {}", win.error());
        exit(1);
    }
    wsi = std::move(*win);
}

/**
 * @brief Initialize the rendering hardware interface.
 */
void Application::init_graphics()
{
    // initialize RHI (rendering hardware interface)
    rhi = lyra::execute([&]() {
        auto desc           = RHIDescriptor{};
        desc.backend        = descriptor.rhi.backend;
        desc.custom_backend = descriptor.rhi.custom_backend;
        desc.flags          = descriptor.rhi.flags;
        desc.window         = *wsi;
        return RHI::init(desc);
    });

    // initialize GPU adatper
    adapter = lyra::execute([&]() {
        auto desc = GPUAdapterDescriptor{};
        return rhi->request_adapter(desc);
    });

    // initialize GPU device
    device = lyra::execute([&]() {
        auto desc  = GPUDeviceDescriptor{};
        desc.label = "main_device";
        return adapter.request_device(desc);
    });

    // initialize GPU surface/swapchain
    surface = lyra::execute([&]() {
        auto desc         = GPUSurfaceDescriptor{};
        desc.label        = "main_surface";
        desc.window       = *wsi;
        desc.present_mode = GPUPresentMode::Fifo;
        desc.frames       = descriptor.rhi.frames;
        return rhi->request_surface(desc);
    });
}

/**
 * @brief Initialize the shader language compiler.
 */
void Application::init_compiler()
{
    // initialize SLC (shader langauge compiler)
    slc = lyra::execute([&]() {
        auto desc      = CompilerDescriptor{};
        desc.target    = resolve_compile_target(descriptor.slc.target, descriptor.rhi.backend);
        desc.flags     = descriptor.slc.flags;
        desc.fs_loader = get_file_loader();

        Vector<CString> includes = descriptor.slc.includes;
        if (desc.fs_loader) {
            bool has_sys = false;
            bool has_app = false;
            for (const auto& m : descriptor.vfs.mounts) {
                if (m.scheme.starts_with("sys://")) has_sys = true;
                if (m.scheme.starts_with("app://")) has_app = true;
            }
            if (has_sys) {
                bool found = false;
                for (auto inc : includes) {
                    if (std::strcmp(inc, "sys://Shaders") == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) includes.push_back("sys://Shaders");
            }
            if (has_app) {
                bool found = false;
                for (auto inc : includes) {
                    if (std::strcmp(inc, "app://Shaders") == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) includes.push_back("app://Shaders");
            }
        }
        desc.includes = includes;
        return Compiler::init(desc);
    });
}

/**
 * @brief Initialize the job system scheduler.
 */
void Application::init_job_system()
{
    JobScheduler::init(descriptor.jobs);
}

/**
 * @brief Bind window callbacks to application methods.
 */
void Application::bind_events()
{
    // bind window callbacks
    wsi->bind<WindowEvent::START, &Application::init>(*this);
    wsi->bind<WindowEvent::UPDATE, &Application::update>(*this);
    wsi->bind<WindowEvent::RENDER, &Application::render>(*this);
    wsi->bind<WindowEvent::RESIZE, &Application::resize>(*this);
    wsi->bind<WindowEvent::CLOSE, &Application::destroy>(*this);
}

/**
 * @brief Handle application initialization event.
 */
void Application::init(const Window&)
{
    run_callbacks<AppEvent::INIT>();
}

/**
 * @brief Handle application update event, including UI stages.
 */
void Application::update(const Window&)
{
    JobScheduler::drain_main_thread();

    run_callbacks<AppEvent::UI_PRE>();
    run_callbacks<AppEvent::UI>();
    run_callbacks<AppEvent::UI_POST>();

    run_callbacks<AppEvent::UPDATE_PRE>();
    run_callbacks<AppEvent::UPDATE>();
    run_callbacks<AppEvent::UPDATE_POST>();

    JobScheduler::drain_main_thread();
}

/**
 * @brief Handle application rendering event.
 */
void Application::render(const Window&)
{
    JobScheduler::drain_main_thread();

    run_callbacks<AppEvent::RENDER_PRE>();
    run_callbacks<AppEvent::RENDER>();
    run_callbacks<AppEvent::RENDER_POST>();
}

/**
 * @brief Handle application resize event.
 */
void Application::resize(const Window&)
{
    run_callbacks<AppEvent::RESIZE>();
}

/**
 * @brief Handle application destruction event.
 */
void Application::destroy(const Window&)
{
    uint  index = static_cast<uint>(AppEvent::DESTROY);
    auto& funcs = callbacks.at(index);
    for (auto it = funcs.rbegin(); it != funcs.rend(); it++)
        (*it)(context);
}

/**
 * @brief Main execution entry point.
 */
void Application::run()
{
    // run event loop
    EventLoop::bind(*wsi);
    EventLoop::run();
}

#pragma endregion AppDescriptor
