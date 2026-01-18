// windows native window
#include <algorithm>
#ifdef USE_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

// macos native window
#ifdef USE_PLATFORM_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

// linux native window
#ifdef USE_PLATFORM_LINUX
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <Lyra/Common/String.h>
#include <Lyra/Common/Logger.h>
#include <Lyra/Common/Plugin.h>
#include <Lyra/Plugin/WSI/WSIAPI.h>
#include <Lyra/Plugin/WSI/WSITypes.h>
#include <Lyra/Plugin/RHI/RHITypes.h>

using namespace lyra;

static Logger logger = create_logger("GLFW", LogLevel::trace);

static Logger get_logger()
{
    return logger;
}

struct UserState
{
    WindowCallback   callback;
    WindowInputQuery events;

    // temporary storage for file paths (file drop event)
    Vector<CString> file_paths = {};

    void reset_events()
    {
        events.num_events = 0;

        file_paths.clear();
    }

    bool is_event_queue_full() const
    {
        return events.num_events >= events.input_events.size();
    }

    void add_mouse_move_event(float xpos, float ypos)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore mouse move event beacause event queue is full!");
            return;
        }

        auto& event           = events.input_events[events.num_events++];
        event.type            = WindowInputEvent::Type::MOUSE_MOVE;
        event.mouse_move.xpos = xpos;
        event.mouse_move.ypos = ypos;
    }

    void add_mouse_wheel_event(float x, float y)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore mouse wheel event beacause event queue is full!");
            return;
        }

        auto& event         = events.input_events[events.num_events++];
        event.type          = WindowInputEvent::Type::MOUSE_WHEEL;
        event.mouse_wheel.x = x;
        event.mouse_wheel.y = y;
    }

    void add_window_focus_event(bool focus)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore window focus event beacause event queue is full!");
            return;
        }

        auto& event                = events.input_events[events.num_events++];
        event.type                 = WindowInputEvent::Type::WINDOW_FOCUS;
        event.window_focus.focused = focus;
    }

    void add_window_close_event()
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore window close event beacause event queue is full!");
            return;
        }

        auto& event = events.input_events[events.num_events++];
        event.type  = WindowInputEvent::Type::WINDOW_CLOSE;
    }

    void add_window_move_event(int xpos, int ypos)
    {
        if (is_event_queue_full()) {
            // input query loop will freeze when window moves,
            // therefore we just keep overwriting the last event.
            events.num_events = static_cast<uint>(events.input_events.size() - 1);
            return;
        }

        auto& event            = events.input_events[events.num_events++];
        event.type             = WindowInputEvent::Type::WINDOW_MOVE;
        event.window_move.xpos = static_cast<uint>(std::max(0, xpos));
        event.window_move.ypos = static_cast<uint>(std::max(0, ypos));
    }

    void add_win_resize_event(int width, int height)
    {
        if (is_event_queue_full()) {
            // input query loop will freeze when window resizes,
            // therefore we just keep overwriting the last event.
            events.num_events = static_cast<uint>(events.input_events.size() - 1);
            return;
        }

        auto& event                = events.input_events[events.num_events++];
        event.type                 = WindowInputEvent::Type::WINDOW_RESIZE;
        event.window_resize.width  = static_cast<uint>(std::max(0, width));
        event.window_resize.height = static_cast<uint>(std::max(0, height));
    }

    void add_mouse_event(MouseButton key, ButtonState state)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore mouse event beacause event queue is full!");
            return;
        }

        auto& event               = events.input_events[events.num_events++];
        event.type                = WindowInputEvent::Type::MOUSE_BUTTON;
        event.mouse_button.button = key;
        event.mouse_button.state  = state;
    }

    void add_keyboard_event(KeyButton key, ButtonState state)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore keyboard event beacause event queue is full!");
            return;
        }

        auto& event             = events.input_events[events.num_events++];
        event.type              = WindowInputEvent::Type::KEY_BUTTON;
        event.key_button.button = key;
        event.key_button.state  = state;
    }

    void add_character_event(uint code)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore character event beacause event queue is full!");
            return;
        }

        auto& event           = events.input_events[events.num_events++];
        event.type            = WindowInputEvent::Type::KEY_TYPING;
        event.key_typing.code = code;
    }

    void add_file_drop_event(uint count, CString* paths)
    {
        if (is_event_queue_full()) {
            get_logger()->warn("Ignore character event beacause event queue is full!");
            return;
        }

        // update file path storage
        file_paths.resize(count);
        for (uint i = 0; i < count; i++)
            file_paths[i] = paths[i];

        auto& event           = events.input_events[events.num_events++];
        event.type            = WindowInputEvent::Type::FILE_DROP;
        event.file_drop.count = count;
        event.file_drop.files = file_paths.data();
    }
};

struct EventLoopInternal
{
    Vector<WindowHandle> windows;
    Deque<WindowHandle>  deferred_window_creation;
    Deque<WindowHandle>  deferred_window_deletion;

    bool should_exit() const { return windows.empty(); }

    void defer_create(const WindowHandle& window)
    {
        deferred_window_creation.push_back(window);
    }

    void defer_delete(const WindowHandle& window)
    {
        deferred_window_deletion.push_back(window);
    }

    void update_windows()
    {
        if (!deferred_window_creation.empty()) {
            auto& window = deferred_window_creation.front();
            windows.push_back(window);
            deferred_window_creation.pop_front();
        }

        if (!deferred_window_deletion.empty()) {
            auto& window = deferred_window_deletion.front();
            cleanup_window(window);
            deferred_window_deletion.pop_front();
        }
    }

    void cleanup_window(const WindowHandle& window)
    {
        auto handle = reinterpret_cast<GLFWwindow*>(window.window);

        // find corresponding window handle
        auto it = std::find_if(
            windows.begin(),
            windows.end(),
            [&](const WindowHandle& window) {
            return handle == reinterpret_cast<GLFWwindow*>(window.window);
        });

        // only delete when window is active
        if (it != windows.end()) {
            auto user = reinterpret_cast<UserState*>(glfwGetWindowUserPointer(handle));

            // remove window from tracking
            windows.erase(it);

            // destroy window pointer
            glfwDestroyWindow(handle);

            // call user close event
            if (user->callback.is_valid())
                user->callback(WindowEvent::CLOSE);

            // delete window user pointer
            delete user;
        }
    }
};

static EventLoopInternal global_event_loop;

#ifdef USE_PLATFORM_WINDOWS
static void create_window_handle(GLFWwindow* win, WindowHandle& window)
{
    window.window = win;
    window.native = glfwGetWin32Window(win);
}
#endif

#ifdef USE_PLATFORM_MACOS
void* get_metal_layer(GLFWwindow* win, void* window);

static void create_window_handle(GLFWwindow* win, WindowHandle& window)
{
    window.window = win;
    window.native = get_metal_layer(win, glfwGetCocoaWindow(win));
}
#endif

#ifdef USE_PLATFORM_LINUX
static void create_window_handle(GLFWwindow* win, WindowHandle& window)
{
    window.window = win;
    window.native = glfwGetX11Window(win);
}
#endif

static ButtonState to_button_state(int action)
{
    switch (action) {
        case GLFW_PRESS:
            return ButtonState::ON;
        case GLFW_RELEASE:
            return ButtonState::OFF;
        case GLFW_REPEAT:
            return ButtonState::ON;
    }
    throw std::invalid_argument("Unsupport GLFW keyboard action!");
}

static auto get_api_name() -> CString
{
    return "Window";
}

static void error_callback(int error, const char* description)
{
    get_logger()->error(description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto& user = *static_cast<UserState*>(glfwGetWindowUserPointer(window));

    // clang-format off
    switch (key) {
        // misc keys
        case GLFW_KEY_TAB:           user.add_keyboard_event(KeyButton::TAB           , to_button_state(action)); break;
        case GLFW_KEY_ESCAPE:        user.add_keyboard_event(KeyButton::ESC           , to_button_state(action)); break;
        case GLFW_KEY_SPACE:         user.add_keyboard_event(KeyButton::SPACE         , to_button_state(action)); break;
        case GLFW_KEY_BACKSPACE:     user.add_keyboard_event(KeyButton::BACKSPACE     , to_button_state(action)); break;
        case GLFW_KEY_DELETE:        user.add_keyboard_event(KeyButton::DEL           , to_button_state(action)); break;
        case GLFW_KEY_ENTER:         user.add_keyboard_event(KeyButton::ENTER         , to_button_state(action)); break;
        case GLFW_KEY_PAGE_UP:       user.add_keyboard_event(KeyButton::PAGE_UP       , to_button_state(action)); break;
        case GLFW_KEY_PAGE_DOWN:     user.add_keyboard_event(KeyButton::PAGE_DOWN     , to_button_state(action)); break;
        case GLFW_KEY_HOME:          user.add_keyboard_event(KeyButton::HOME          , to_button_state(action)); break;
        case GLFW_KEY_END:           user.add_keyboard_event(KeyButton::END           , to_button_state(action)); break;
        case GLFW_KEY_PAUSE:         user.add_keyboard_event(KeyButton::PAUSE         , to_button_state(action)); break;
        case GLFW_KEY_NUM_LOCK:      user.add_keyboard_event(KeyButton::NUM_LOCK      , to_button_state(action)); break;
        case GLFW_KEY_CAPS_LOCK:     user.add_keyboard_event(KeyButton::CAPS_LOCK     , to_button_state(action)); break;
        case GLFW_KEY_SCROLL_LOCK:   user.add_keyboard_event(KeyButton::SCROLL_LOCK   , to_button_state(action)); break;
        case GLFW_KEY_PRINT_SCREEN:  user.add_keyboard_event(KeyButton::PRINT_SCREEN  , to_button_state(action)); break;

        // mod keys
        case GLFW_KEY_LEFT_ALT:      user.add_keyboard_event(KeyButton::ALT           , to_button_state(action)); break;
        case GLFW_KEY_RIGHT_ALT:     user.add_keyboard_event(KeyButton::ALT           , to_button_state(action)); break;
        case GLFW_KEY_LEFT_CONTROL:  user.add_keyboard_event(KeyButton::CTRL          , to_button_state(action)); break;
        case GLFW_KEY_RIGHT_CONTROL: user.add_keyboard_event(KeyButton::CTRL          , to_button_state(action)); break;
        case GLFW_KEY_LEFT_SHIFT:    user.add_keyboard_event(KeyButton::SHIFT         , to_button_state(action)); break;
        case GLFW_KEY_RIGHT_SHIFT:   user.add_keyboard_event(KeyButton::SHIFT         , to_button_state(action)); break;
        case GLFW_KEY_LEFT_SUPER:    user.add_keyboard_event(KeyButton::SUPER         , to_button_state(action)); break;
        case GLFW_KEY_RIGHT_SUPER:   user.add_keyboard_event(KeyButton::SUPER         , to_button_state(action)); break;

        // arrow keys
        case GLFW_KEY_UP:            user.add_keyboard_event(KeyButton::UP            , to_button_state(action)); break;
        case GLFW_KEY_DOWN:          user.add_keyboard_event(KeyButton::DOWN          , to_button_state(action)); break;
        case GLFW_KEY_LEFT:          user.add_keyboard_event(KeyButton::LEFT          , to_button_state(action)); break;
        case GLFW_KEY_RIGHT:         user.add_keyboard_event(KeyButton::RIGHT         , to_button_state(action)); break;

        // numpad keys
        case GLFW_KEY_0:             user.add_keyboard_event(KeyButton::D0            , to_button_state(action)); break;
        case GLFW_KEY_1:             user.add_keyboard_event(KeyButton::D1            , to_button_state(action)); break;
        case GLFW_KEY_2:             user.add_keyboard_event(KeyButton::D2            , to_button_state(action)); break;
        case GLFW_KEY_3:             user.add_keyboard_event(KeyButton::D3            , to_button_state(action)); break;
        case GLFW_KEY_4:             user.add_keyboard_event(KeyButton::D4            , to_button_state(action)); break;
        case GLFW_KEY_5:             user.add_keyboard_event(KeyButton::D5            , to_button_state(action)); break;
        case GLFW_KEY_6:             user.add_keyboard_event(KeyButton::D6            , to_button_state(action)); break;
        case GLFW_KEY_7:             user.add_keyboard_event(KeyButton::D7            , to_button_state(action)); break;
        case GLFW_KEY_8:             user.add_keyboard_event(KeyButton::D8            , to_button_state(action)); break;
        case GLFW_KEY_9:             user.add_keyboard_event(KeyButton::D9            , to_button_state(action)); break;

        // function keys
        case GLFW_KEY_F1:            user.add_keyboard_event(KeyButton::F1            , to_button_state(action)); break;
        case GLFW_KEY_F2:            user.add_keyboard_event(KeyButton::F2            , to_button_state(action)); break;
        case GLFW_KEY_F3:            user.add_keyboard_event(KeyButton::F3            , to_button_state(action)); break;
        case GLFW_KEY_F4:            user.add_keyboard_event(KeyButton::F4            , to_button_state(action)); break;
        case GLFW_KEY_F5:            user.add_keyboard_event(KeyButton::F5            , to_button_state(action)); break;
        case GLFW_KEY_F6:            user.add_keyboard_event(KeyButton::F6            , to_button_state(action)); break;
        case GLFW_KEY_F7:            user.add_keyboard_event(KeyButton::F7            , to_button_state(action)); break;
        case GLFW_KEY_F8:            user.add_keyboard_event(KeyButton::F8            , to_button_state(action)); break;
        case GLFW_KEY_F9:            user.add_keyboard_event(KeyButton::F9            , to_button_state(action)); break;
        case GLFW_KEY_F10:           user.add_keyboard_event(KeyButton::F10           , to_button_state(action)); break;
        case GLFW_KEY_F11:           user.add_keyboard_event(KeyButton::F11           , to_button_state(action)); break;
        case GLFW_KEY_F12:           user.add_keyboard_event(KeyButton::F12           , to_button_state(action)); break;

        // character keys
        case GLFW_KEY_A:             user.add_keyboard_event(KeyButton::A             , to_button_state(action)); break;
        case GLFW_KEY_B:             user.add_keyboard_event(KeyButton::B             , to_button_state(action)); break;
        case GLFW_KEY_C:             user.add_keyboard_event(KeyButton::C             , to_button_state(action)); break;
        case GLFW_KEY_D:             user.add_keyboard_event(KeyButton::D             , to_button_state(action)); break;
        case GLFW_KEY_E:             user.add_keyboard_event(KeyButton::E             , to_button_state(action)); break;
        case GLFW_KEY_F:             user.add_keyboard_event(KeyButton::F             , to_button_state(action)); break;
        case GLFW_KEY_G:             user.add_keyboard_event(KeyButton::G             , to_button_state(action)); break;
        case GLFW_KEY_H:             user.add_keyboard_event(KeyButton::H             , to_button_state(action)); break;
        case GLFW_KEY_I:             user.add_keyboard_event(KeyButton::I             , to_button_state(action)); break;
        case GLFW_KEY_J:             user.add_keyboard_event(KeyButton::J             , to_button_state(action)); break;
        case GLFW_KEY_K:             user.add_keyboard_event(KeyButton::K             , to_button_state(action)); break;
        case GLFW_KEY_L:             user.add_keyboard_event(KeyButton::L             , to_button_state(action)); break;
        case GLFW_KEY_M:             user.add_keyboard_event(KeyButton::M             , to_button_state(action)); break;
        case GLFW_KEY_N:             user.add_keyboard_event(KeyButton::N             , to_button_state(action)); break;
        case GLFW_KEY_O:             user.add_keyboard_event(KeyButton::O             , to_button_state(action)); break;
        case GLFW_KEY_P:             user.add_keyboard_event(KeyButton::P             , to_button_state(action)); break;
        case GLFW_KEY_Q:             user.add_keyboard_event(KeyButton::Q             , to_button_state(action)); break;
        case GLFW_KEY_R:             user.add_keyboard_event(KeyButton::R             , to_button_state(action)); break;
        case GLFW_KEY_S:             user.add_keyboard_event(KeyButton::S             , to_button_state(action)); break;
        case GLFW_KEY_T:             user.add_keyboard_event(KeyButton::T             , to_button_state(action)); break;
        case GLFW_KEY_U:             user.add_keyboard_event(KeyButton::U             , to_button_state(action)); break;
        case GLFW_KEY_V:             user.add_keyboard_event(KeyButton::V             , to_button_state(action)); break;
        case GLFW_KEY_W:             user.add_keyboard_event(KeyButton::W             , to_button_state(action)); break;
        case GLFW_KEY_X:             user.add_keyboard_event(KeyButton::X             , to_button_state(action)); break;
        case GLFW_KEY_Y:             user.add_keyboard_event(KeyButton::Y             , to_button_state(action)); break;
        case GLFW_KEY_Z:             user.add_keyboard_event(KeyButton::Z             , to_button_state(action)); break;

        // non-character ascii keys
        case GLFW_KEY_APOSTROPHE:    user.add_keyboard_event(KeyButton::APOSTROPHE    , to_button_state(action)); break;
        case GLFW_KEY_COMMA:         user.add_keyboard_event(KeyButton::COMMA         , to_button_state(action)); break;
        case GLFW_KEY_MINUS:         user.add_keyboard_event(KeyButton::MINUS         , to_button_state(action)); break;
        case GLFW_KEY_PERIOD:        user.add_keyboard_event(KeyButton::PERIOD        , to_button_state(action)); break;
        case GLFW_KEY_SLASH:         user.add_keyboard_event(KeyButton::SLASH         , to_button_state(action)); break;
        case GLFW_KEY_BACKSLASH:     user.add_keyboard_event(KeyButton::BACKSLASH     , to_button_state(action)); break;
        case GLFW_KEY_SEMICOLON:     user.add_keyboard_event(KeyButton::SEMICOLON     , to_button_state(action)); break;
        case GLFW_KEY_EQUAL:         user.add_keyboard_event(KeyButton::EQUAL         , to_button_state(action)); break;
        case GLFW_KEY_LEFT_BRACKET:  user.add_keyboard_event(KeyButton::LEFT_BRACKET  , to_button_state(action)); break;
        case GLFW_KEY_RIGHT_BRACKET: user.add_keyboard_event(KeyButton::RIGHT_BRACKET , to_button_state(action)); break;
        case GLFW_KEY_GRAVE_ACCENT:  user.add_keyboard_event(KeyButton::GRAVE_ACCENT  , to_button_state(action)); break;

        default: get_logger()->warn("Unhandled key stroke!");
    }
    // clang-format off
}

static void file_drop_callback(GLFWwindow *window, int path_count, CString paths[])
{
    auto& user  = *static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user.add_file_drop_event(path_count, paths);
}

static void key_character_callback(GLFWwindow* window, uint code)
{
    auto& user  = *static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user.add_character_event(code);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    auto& user  = *static_cast<UserState*>(glfwGetWindowUserPointer(window));

    // clang-format off
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:   user.add_mouse_event(MouseButton::LEFT,   to_button_state(action)); break;
        case GLFW_MOUSE_BUTTON_MIDDLE: user.add_mouse_event(MouseButton::MIDDLE, to_button_state(action)); break;
        case GLFW_MOUSE_BUTTON_RIGHT:  user.add_mouse_event(MouseButton::RIGHT,  to_button_state(action)); break;
        default: get_logger()->warn("Unhandled mouse button click!");
    }
    // clang-format on
}

static void mouse_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    auto& user = *static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user.add_mouse_move_event(static_cast<float>(xpos), static_cast<float>(ypos));
}

static void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto& user = *static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user.add_mouse_wheel_event(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

static void window_close_callback(GLFWwindow* window)
{
    // clean up user states
    auto user = static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user->add_window_close_event();

    auto handle   = WindowHandle{};
    handle.window = window;
    handle.native = nullptr;
    global_event_loop.defer_delete(handle);
}

static void window_position_callback(GLFWwindow* window, int xpos, int ypos)
{
    // clean up user states
    auto user = static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user->add_window_move_event(xpos, ypos);
}

static void window_focus_callback(GLFWwindow* window, int focused)
{
    // clean up user states
    auto user = static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user->add_window_focus_event(focused);
}

static void window_resize_callback(GLFWwindow* window, int width, int height)
{
    // clean up user states
    auto user = static_cast<UserState*>(glfwGetWindowUserPointer(window));
    user->add_win_resize_event(width, height);

    // call user callback function
    if (user->callback.is_valid())
        std::invoke(user->callback, WindowEvent::RESIZE);
}

static void bind_window_events(WindowHandle window)
{
    auto user    = new UserState{};
    user->events = {};

    // callback
    auto handle = reinterpret_cast<GLFWwindow*>(window.window);
    glfwSetWindowUserPointer(handle, user);
    glfwSetDropCallback(handle, file_drop_callback);
    glfwSetKeyCallback(handle, key_callback);
    glfwSetCharCallback(handle, key_character_callback);
    glfwSetCursorPosCallback(handle, mouse_position_callback);
    glfwSetMouseButtonCallback(handle, mouse_button_callback);
    glfwSetScrollCallback(handle, mouse_scroll_callback);
    glfwSetWindowPosCallback(handle, window_position_callback);
    glfwSetWindowFocusCallback(handle, window_focus_callback);
    glfwSetWindowSizeCallback(handle, window_resize_callback);
    glfwSetWindowCloseCallback(handle, window_close_callback);
}

static bool create_window(const WindowDescriptor& desc, WindowHandle& window)
{
    // set window extent
    uint width  = desc.width;
    uint height = desc.height;

    // set window monitor
    GLFWmonitor* monitor = nullptr;

    // check full screen requirement
    if (desc.flags.contains(WindowFlag::FULLSCREEN)) {
        monitor                 = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        width                   = mode->width;
        height                  = mode->height;
    }

    // no OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // set the window hint for no decorations
    glfwWindowHint(GLFW_DECORATED, desc.flags.contains(WindowFlag::DECORATED) ? GLFW_TRUE : GLFW_FALSE);

    // set the window hint for maximized
    glfwWindowHint(GLFW_MAXIMIZED, desc.flags.contains(WindowFlag::MAXIMIZED) ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* win = glfwCreateWindow(width, height, desc.title, monitor, NULL);
    if (win != nullptr) {
        create_window_handle(win, window);
        bind_window_events(window);
        global_event_loop.defer_create(window);
    }
    return true;
}

static void delete_window(WindowHandle window)
{
    global_event_loop.defer_delete(window);
}

static void set_window_pos(WindowHandle window, int x, int y)
{
    glfwSetWindowPos((GLFWwindow*)window.window, x, y);
}

static void get_window_pos(WindowHandle window, int& x, int& y)
{
    glfwGetWindowPos((GLFWwindow*)window.window, &x, &y);
}

static void set_window_size(WindowHandle window, uint width, uint height)
{
    glfwSetWindowSize((GLFWwindow*)window.window, width, height);
}

static void get_window_size(WindowHandle window, uint& width, uint& height)
{
    int32_t w, h;
    glfwGetWindowSize((GLFWwindow*)window.window, &w, &h);
    width  = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);
}

// NOTE: from ImGui glfw implementation:
// - On Windows the process needs to be marked DPI-aware!! SDL2 doesn't do it by default. You can call ::SetProcessDPIAware() from Win32 backend.
// - Apple platforms use FramebufferScale so we always return 1.0f.
// - Some accessibility applications are declaring virtual monitors with a DPI of 0.0f, see #7902. We preserve this value for caller to handle.
static void get_content_scale(WindowHandle window, float& xscale, float& yscale)
{
#if !(defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__ANDROID__))
    glfwGetWindowContentScale((GLFWwindow*)window.window, &xscale, &yscale);
#else
    (void)window;
    xscale = 1.0f;
    yscale = 1.0f;
#endif
}

static void get_framebuffer_scale(WindowHandle window, float& xscale, float& yscale)
{
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize((GLFWwindow*)window.window, &w, &h);
    glfwGetFramebufferSize((GLFWwindow*)window.window, &display_w, &display_h);
    if (w > 0 && h > 0) {
        xscale = static_cast<float>(display_w) / static_cast<float>(w);
        yscale = static_cast<float>(display_h) / static_cast<float>(h);
    } else {
        xscale = 1.0f;
        yscale = 1.0f;
    }
}

static void set_window_focus(WindowHandle window)
{
    glfwFocusWindow((GLFWwindow*)window.window);
}

static bool get_window_focus(WindowHandle window)
{
    return glfwGetWindowAttrib((GLFWwindow*)window.window, GLFW_FOCUSED);
}

static void set_window_alpha(WindowHandle window, float alpha)
{
    glfwSetWindowOpacity((GLFWwindow*)window.window, alpha);
}

static float get_window_alpha(WindowHandle window)
{
    return glfwGetWindowOpacity((GLFWwindow*)window.window);
}

static void set_window_title(WindowHandle window, CString title)
{
    glfwSetWindowTitle((GLFWwindow*)window.window, title);
}

static CString get_window_title(WindowHandle window)
{
#if defined(_WIN32)
    return glfwGetWindowTitle((GLFWwindow*)window.window);
#else
    return "";
#endif
}

static void set_clipboard_text(WindowHandle window, CString text)
{
    glfwSetClipboardString((GLFWwindow*)window.window, text);
}

static CString get_clipboard_text(WindowHandle window)
{
    return glfwGetClipboardString((GLFWwindow*)window.window);
}

static bool get_window_minimized(WindowHandle window)
{
    return glfwGetWindowAttrib((GLFWwindow*)window.window, GLFW_ICONIFIED);
}

static void query_input_events(WindowHandle window, WindowInputQuery& query)
{
    auto  handle = reinterpret_cast<GLFWwindow*>(window.window);
    auto& user   = *static_cast<UserState*>(glfwGetWindowUserPointer(handle));

    // copy the input state back to user
    query = user.events;
}

static void show_window(WindowHandle window)
{
    glfwShowWindow(reinterpret_cast<GLFWwindow*>(window.window));
}

static void bind_window_callback(WindowHandle window, WindowCallback callback)
{
    auto  handle  = reinterpret_cast<GLFWwindow*>(window.window);
    auto& user    = *static_cast<UserState*>(glfwGetWindowUserPointer(handle));
    user.callback = callback;
}

static void run_in_loop()
{
    // create/destroy windows in the deferred deletion queue
    global_event_loop.update_windows();

    // START
    for (auto& window : global_event_loop.windows) {
        auto handle = reinterpret_cast<GLFWwindow*>(window.window);
        auto user   = static_cast<UserState*>(glfwGetWindowUserPointer(handle));
        if (user->callback.is_valid())
            std::invoke(user->callback, WindowEvent::START);
    }

    // glfw main loop
    while (!global_event_loop.should_exit()) {

        // create/destroy windows in the deferred deletion queue
        global_event_loop.update_windows();

        // UPDATE
        for (auto& window : global_event_loop.windows) {
            auto handle = reinterpret_cast<GLFWwindow*>(window.window);
            auto user   = static_cast<UserState*>(glfwGetWindowUserPointer(handle));
            if (user->callback.is_valid())
                std::invoke(user->callback, WindowEvent::UPDATE);
        }

        // RENDER
        RHI::new_frame(); // prior to frame begin
        for (auto& window : global_event_loop.windows) {
            auto handle = reinterpret_cast<GLFWwindow*>(window.window);
            auto user   = static_cast<UserState*>(glfwGetWindowUserPointer(handle));
            if (user->callback.is_valid())
                std::invoke(user->callback, WindowEvent::RENDER);
        }
        RHI::end_frame(); // post frame end

        // RESET EVENTS
        for (auto& window : global_event_loop.windows) {
            auto handle = reinterpret_cast<GLFWwindow*>(window.window);
            auto user   = static_cast<UserState*>(glfwGetWindowUserPointer(handle));
            user->reset_events();
        }

        glfwPollEvents();
    }

    // wait until GPU device is idle
    RHI::wait();
}

static void list_monitors(uint& count, MonitorInfo* monitor_infos)
{
    static_assert(sizeof(uint) == sizeof(int));

    auto monitors = glfwGetMonitors(reinterpret_cast<int*>(&count));
    if (monitor_infos == nullptr) return;

    for (uint i = 0; i < count; i++) {
        auto  monitor = monitors[i];
        auto& info    = monitor_infos[i];

        const GLFWvidmode* vid_mode = glfwGetVideoMode(monitor);
        if (vid_mode == nullptr)
            continue; // failed to get video mode (e.g. Emscripten does not support this function)

        int monitor_pos_x, monitor_pos_y;
        glfwGetMonitorPos(monitor, &monitor_pos_x, &monitor_pos_y);

        int workarea_pos_x, workarea_pos_y;
        int workarea_width, workarea_height;
        glfwGetMonitorWorkarea(monitor, &workarea_pos_x, &workarea_pos_y, &workarea_width, &workarea_height);

        float scale_x, scale_y;
        glfwGetMonitorContentScale(monitor, &scale_x, &scale_y);

        info.monitor_pos_x  = monitor_pos_x;
        info.monitor_pos_y  = monitor_pos_y;
        info.monitor_width  = vid_mode->width;
        info.monitor_height = vid_mode->height;

        // workaround a small GLFW issue reporting zero on monitor changes: https://github.com/glfw/glfw/pull/1761
        if (workarea_width > 0 && workarea_height > 0) {
            info.workarea_pos_x  = workarea_pos_x;
            info.workarea_pos_y  = workarea_pos_y;
            info.workarea_width  = workarea_width;
            info.workarea_height = workarea_height;
        } else {
            info.workarea_pos_x  = info.monitor_pos_x;
            info.workarea_pos_y  = info.monitor_pos_y;
            info.workarea_width  = info.monitor_width;
            info.workarea_height = info.monitor_height;
        }

        info.dpi_scale_x = scale_x;
        info.dpi_scale_y = scale_y;
    }
}

LYRA_EXPORT auto prepare() -> void
{
    get_logger()->set_level(parse_log_level_from_env("LYRA_GLFW_VERBOSITY"));

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(error_callback);
}

LYRA_EXPORT auto cleanup() -> void
{
    glfwTerminate();
}

LYRA_EXPORT auto create() -> WindowAPI
{
    auto api                  = WindowAPI{};
    api.get_api_name          = get_api_name;
    api.list_monitors         = list_monitors;
    api.create_window         = create_window;
    api.delete_window         = delete_window;
    api.set_window_pos        = set_window_pos;
    api.get_window_pos        = get_window_pos;
    api.set_window_size       = set_window_size;
    api.get_window_size       = get_window_size;
    api.get_content_scale     = get_content_scale;
    api.get_framebuffer_scale = get_framebuffer_scale;
    api.set_window_focus      = set_window_focus;
    api.get_window_focus      = get_window_focus;
    api.set_window_alpha      = set_window_alpha;
    api.get_window_alpha      = get_window_alpha;
    api.set_window_title      = set_window_title;
    api.get_window_title      = get_window_title;
    api.set_clipboard_text    = set_clipboard_text;
    api.get_clipboard_text    = get_clipboard_text;
    api.get_window_minimized  = get_window_minimized;
    api.query_input_events    = query_input_events;
    api.bind_window_callback  = bind_window_callback;
    api.show_window           = show_window;
    api.run_in_loop           = run_in_loop;
    return api;
}
