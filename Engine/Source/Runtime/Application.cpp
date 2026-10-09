#include <Lyra/Runtime/Application.h>
#include <Lyra/JobSystem/Jobs.h>

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
    switch (backend) {
        case RHIBackend::D3D12:
            slc.target = CompileTarget::DXIL;
            slc.flags |= CompileFlag::REFLECT; // reflect is always on
            break;
        case RHIBackend::METAL:
            slc.target = CompileTarget::MSL;
            slc.flags |= CompileFlag::REFLECT; // reflect is always on
            break;
        default:
            slc.target = CompileTarget::SPIRV;
            slc.flags |= CompileFlag::REFLECT; // reflect is always on
            break;
    }
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
#pragma endregion AppDescriptor

#pragma region Application
/**
 * @brief Initialize the application.
 */
Application::Application(const AppDescriptor& descriptor)
    : descriptor(descriptor)
{
    init_logger();
    init_job_system();
    init_window();
    init_graphics();
    init_compiler();
    bind_events();

    // adding commonly used components into toolboard
    context.toolboard.add<Application*>(this);
    context.toolboard.add<Window*>(wsi.get());
    context.toolboard.add<Compiler*>(slc.get());
    context.toolboard.add<GPUAdapter*>(&adapter);
    context.toolboard.add<GPUDevice*>(&device);
    context.toolboard.add<GPUSurface*>(&surface);
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
    wsi = Window::init(descriptor.wsi);
}

/**
 * @brief Initialize the rendering hardware interface.
 */
void Application::init_graphics()
{
    // initialize RHI (rendering hardware interface)
    rhi = lyra::execute([&]() {
        auto desc    = RHIDescriptor{};
        desc.backend = descriptor.rhi.backend;
        desc.flags   = descriptor.rhi.flags;
        desc.window  = *wsi;
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
        auto desc   = CompilerDescriptor{};
        desc.target = descriptor.slc.target;
        desc.flags  = descriptor.slc.flags;
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
