# Overview

**Lyra-Engine** is my attempt to create a rendering-focused engine.

I have always been fond of creating a working rendering engine from scratch.
In the past, I have made multiple attempts but none of them got very far due to
a lack of time and experience. In my previous attempts, I was mostly working with
Vulkan-based projects, and implemented a number of rendering algorithms including
PBR-based shading, deferred rendering, global illumination using surfels, etc.
These were somewhat successful, but they are scattered across different repos and
with Vulkan implementations only. I am aiming to create a common codebase that I
can leverage in future graphics/rendering projects.

## Devlogs

- [RHI Design](RHI.md)
- [Asset Management System](Assets.md)

## Design

The next section covers the architectural structure of **Lyra-Engine**.
The architecture of this project is heavily inspired by [OurMachinery](https://ruby0x1.github.io/machinery_blog_archive/).
Different components are compiled as shared libraries and loaded at runtime using
`LoadLibrary` (on Windows) or `dlopen` (on Unix-styled OS). This design requires
developers to define a set of APIs at an early stage. The implementations of the
API could vary and can be swapped out easily. For example, different RHI backends
(Vulkan, D3D12, Metal) could be implemented in completely separate libraries, loaded
independently based on user's choice.

Unlike **OurMachinery**, I have chosen to use C++ and not strictly follow the C ABI.
This is because doing so introduces too much overhead for a single-developer project.
The downside of not strictly following the C ABI is that it does not guarantee compatibility
between code compiled by different compilers. However, this won't be a problem for me
since I will be compiling both engine and application code on the same machine with the
same compiler. Although I am not planning to strictly follow the C ABI, I will try to
make as many plugins C ABI compatible as possible. With the API defined in terms of a
struct of function pointers, I am implementing a C++ style wrapper on top of the raw
API for usability.

The separation of implementation in different shared libraries also introduces potential
for hot-reloading. However, custom hot-reloading has some inherent difficulties on Windows,
because Windows locks the library and the debugging files (pdb) while the application is
running. This locking mechanism prevents compilation from overwriting the existing library.
There are some workarounds, but currently I have not attempted this.

## Example

For each API definition, we define a struct of function pointers.
To allow simpler handling of API loading/unloading, we provide **Plugin** as a helper.
For example, the following snippet features a simplified version of our **WindowAPI**.

```cpp
struct WindowAPI
{
    // api name
    CString (*get_api_name)();

    bool (*create_window)(const WindowDescriptor& desc, WindowHandle& window);
    void (*delete_window)(WindowHandle window);

    bool (*bind_window_callback)(WindowHandle window, WindowCallback&& callback);

    bool (*get_window_size)(WindowHandle window, uint& width, uint& height);
    bool (*get_input_state)(WindowHandle window, WindowInputState& state);

    void (*run_in_loop)();
};

using WindowPlugin = Plugin<WindowAPI>;
```
