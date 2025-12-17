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

void EventLoop::bind(Window& window)
{
    Window::api()->bind_window_callback(window.handle, [=](WindowEvent event) mutable {
        switch (event) {
            case WindowEvent::START:
                for (auto& cb : window.callbacks.start)
                    cb(window);
                break;
            case WindowEvent::CLOSE:
                // invoke callbacks in reverse order
                for (auto it = window.callbacks.close.rbegin(); it != window.callbacks.close.rend(); it++)
                    (*it)(window);
                break;
            case WindowEvent::TIMER:
                for (auto& cb : window.callbacks.timer)
                    cb(window);
                break;
            case WindowEvent::UPDATE:
                window.inputs.update(window.handle);
                for (auto& cb : window.callbacks.update)
                    cb(window);
                break;
            case WindowEvent::RENDER:
                for (auto& cb : window.callbacks.render)
                    cb(window);
                break;
            case WindowEvent::RESIZE:
                for (auto& cb : window.callbacks.resize)
                    cb(window);
                break;
        }
    });
}

void EventLoop::run()
{
    Window::api()->run_in_loop();
}
