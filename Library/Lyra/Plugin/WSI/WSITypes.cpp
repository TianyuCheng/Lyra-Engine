#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/WSI/WSIAPI.h>
#include <Lyra/Plugin/WSI/WSITypes.h>

using namespace lyra;

using WindowPlugin = Plugin<WindowAPI>;

static Own<WindowPlugin> WINDOW_PLUGIN;

OwnedResource<Window> Window::init(const WindowDescriptor& descriptor)
{
    if (WINDOW_PLUGIN.get()) {
        show_error("WSI", "Call Window::init() exactly once!");
        exit(1);
    }

    WINDOW_PLUGIN = std::make_unique<WindowPlugin>("lyra-glfw");

    OwnedResource<Window> window(new Window());
    Window::api()->create_window(descriptor, window->handle);
    return window;
}

WindowAPI* Window::api()
{
    return WINDOW_PLUGIN->get_api();
}

void Window::loop()
{
    EventLoop::bind(*this);
    EventLoop::run();
}

void Window::destroy()
{
    Window::api()->delete_window(this->handle);
}

void Window::get_position(int& width, int& height) const
{
    Window::api()->get_window_pos(this->handle, width, height);
}

void Window::get_extent(uint& width, uint& height) const
{
    Window::api()->get_window_size(this->handle, width, height);
}

void Window::get_content_scale(float& xscale, float& yscale) const
{
    Window::api()->get_content_scale(this->handle, xscale, yscale);
}

void Window::get_framebuffer_scale(float& xscale, float& yscale) const
{
    Window::api()->get_framebuffer_scale(this->handle, xscale, yscale);
}

void Window::dispatch(WindowEvent event)
{
    switch (event) {
        case WindowEvent::START:
            for (auto& cb : callbacks.start)
                cb(*this);
            break;
        case WindowEvent::CLOSE:
            // invoke callbacks in reverse order
            for (auto it = callbacks.close.rbegin(); it != callbacks.close.rend(); it++)
                (*it)(*this);
            break;
        case WindowEvent::TIMER:
            for (auto& cb : callbacks.timer)
                cb(*this);
            break;
        case WindowEvent::UPDATE:
            inputs.update(handle);
            for (auto& cb : callbacks.update)
                cb(*this);
            break;
        case WindowEvent::RENDER:
            for (auto& cb : callbacks.render)
                cb(*this);
            break;
        case WindowEvent::RESIZE:
            for (auto& cb : callbacks.resize)
                cb(*this);
            break;
    }
}

void EventLoop::bind(Window& window)
{
    auto callback = WindowCallback::create<Window, &Window::dispatch>(window);
    Window::api()->bind_window_callback(window.handle, callback);
}

void EventLoop::run()
{
    Window::api()->run_in_loop();
}
