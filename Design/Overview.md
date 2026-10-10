# Overview

**Lyra-Engine** is my attempt to create a rendering-focused engine.

I have always been fond of creating a working rendering engine from scratch. In the past, I made multiple attempts, but none got very far due to a lack of time and experience. In those attempts, I mostly worked on Vulkan-based projects and implemented rendering techniques including PBR shading, deferred rendering, and surfel-based global illumination. While those projects were functional, they were scattered across separate repositories and tied exclusively to Vulkan. My goal with Lyra is to build a unified codebase that I can leverage across future graphics and rendering projects.

## Documentation

- [Graphics API Design](Graphics.md)
- [Asset Management System](Assets.md)
- [Job System](Jobs.md)
- [Scripting System](Scripting.md)
- [Scene Architecture](Scene.md)
- [Virtual File System](FileSystem.md)

## Architecture

The engine architecture is heavily inspired by [Our Machinery](https://ruby0x1.github.io/machinery_blog_archive/). Subsystems are compiled as shared libraries (or linked as builtins) and loaded through clean API interfaces. This approach forces clear API boundaries early on, making implementations interchangeable. For example, different graphics backends (Vulkan, D3D12, Metal) reside in separate modules and can be selected at runtime.

Unlike Our Machinery, I chose to use C++ rather than strictly sticking to a C ABI. A strict C ABI introduces substantial boilerplate that is unnecessary for a solo developer project where the engine and modules are built with the same compiler toolchain. While not strictly bound to the C ABI, modules still expose their interfaces through C-style structs of function pointers (`create`, `prepare`, `cleanup`). C++ helper classes wrap these raw interfaces to provide convenient, type-safe abstractions.

### Repository Layout

The codebase is organized into several key directories:

- **`Engine/`**: Core engine modules and public headers (`Lyra/`):
  - `Utilities`: Foundation types, memory allocators, math, strings, containers, and plugin loaders.
  - `Graphics`: The Render Hardware Interface (RHI) definitions and types.
  - `Rendering`: The Frame Graph execution system and render pipeline abstractions.
  - `Assets`: The Asset Management System (AMS), registries, loaders, and cookers.
  - `FileSystem`: Virtual File System (VFS) interfaces, package archives, and loaders.
  - `Compiler`: Shader compiler interfaces (Slang).
  - `Windowing`: Window System Interface (WSI) definitions.
  - `InputSystem`: Input devices, mappings, and state management.
  - `JobSystem`: Multithreaded task scheduling and worker pools.
  - `Scene`: World hierarchy, SceneTree, camera, and Flecs ECS integration.
  - `UISystem`: UI renderer abstractions and immediate-mode GUI widgets.
  - `Runtime`: Application lifecycle manager, execution contexts, and layers.
- **`Modules/`**: Implementations loaded as plugins:
  - `Graphics/`: Vulkan, D3D12, and Metal RHI backends.
  - `Windowing/`: GLFW windowing implementation.
  - `Compiler/`: Slang shader compilation and reflection backend.
  - `FileSystem/`: Native filesystem loader and package reader/writer.
  - `UISystem/`: Dear ImGui rendering backend.
  - `Assets/`: Cookers and loaders for meshes, models, materials, textures, and scenes.
- **`Samples/`**: Standalone applications and tools built on top of the engine (e.g., `Samples/Editor`).
- **`Tools/`**: Code generators, configuration scripts, and external vendor dependencies.

### Plugin System

Each module defines a struct of function pointers and exports standard entry points: `create()`, `prepare()`, and `cleanup()`. Modules can be dynamically loaded at runtime using `Plugin<APIType>` or statically wired using `BuiltinPlugin<APIType>`.

For example, the windowing system defines `WindowAPI`:

```cpp
struct WindowAPI
{
    CString (*get_api_name)();

    void (*list_monitors)(uint& count, MonitorInfo* monitors);

    bool (*create_window)(const WindowDescriptor& desc, WindowHandle& window);
    void (*delete_window)(WindowHandle window);

    void (*set_window_pos)(WindowHandle window, int x, int y);
    void (*get_window_pos)(WindowHandle window, int& x, int& y);

    void (*set_window_size)(WindowHandle window, uint width, uint height);
    void (*get_window_size)(WindowHandle window, uint& width, uint& height);

    void (*bind_window_callback)(WindowHandle window, WindowCallback callback);
    void (*query_input_events)(WindowHandle window, WindowInputQuery& query);

    void (*show_window)(WindowHandle window);
    void (*run_in_loop)();
};

using WindowPlugin = Plugin<WindowAPI>;
```

### Application & Runtime Model

The engine runtime is coordinated by `Application`, which drives the main loop and dispatches lifecycle events (`AppEvent`).

State and subsystems are shared through `AppContext`, which cleanly separates hardware from frame data:

- **`Toolboard`**: A non-owning registry for physical hardware, devices, and engine subsystems (such as `GPUDevice`, `WindowAPI`, `SceneManager`, and `ShaderCompiler`).
- **`Blackboard`**: An owning, heterogeneous storage container for transient frame state, scene parameters, and user data.

Applications are composed of modular layers (`RenderLayer`, `AssetLayer`, `SceneLayer`, `InputLayer`, `TimingLayer`, `UILayer`, `ScriptLayer`), each hooking into application lifecycle stages via delegates.

### Application Setup Example

The following snippet demonstrates configuring an `Application`, binding a lifecycle delegate, accessing engine tools through `AppContext`, and running the main loop:

```cpp
#include <Lyra/Runtime/Application.h>
#include <Lyra/Graphics/RHITypes.h>

using namespace lyra;

int main()
{
    AppDescriptor desc = {};
    desc.window.title  = "Lyra Engine";
    desc.window.width  = 1280;
    desc.window.height = 720;

    Application app(desc);

    // bind per-frame update callback
    app.bind<AppEvent::UPDATE>([](AppContext& ctx) {
        // query non-owning subsystems from the toolboard
        auto& device = ctx.tool<GPUDevice>();

        // access or initialize owning state on the blackboard
        struct FrameStats { uint frame_index = 0; };
        auto& stats = ctx.data<FrameStats>();
        stats.frame_index++;
    });

    app.run();
    return 0;
}
```
