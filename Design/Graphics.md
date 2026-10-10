# Graphics API

## API Design

For a rendering engine, the RHI (Render Hardware Interface) serves as the foundation. **Lyra-Engine** supports multiple graphics APIs across platforms (Vulkan, D3D12, Metal), which requires a unified abstraction layer.

In earlier projects, I designed abstraction layers exclusively for Vulkan. While they functioned well, they were tightly coupled to Vulkan idioms and did not translate cleanly to other APIs. To design a more portable abstraction, I chose [WebGPU](https://www.w3.org/TR/webgpu/) as a structural baseline. WebGPU offers a well-defined common denominator across modern graphics APIs, standardizing pipelines, render passes, and group-based binding models.

However, standard WebGPU lacks several capabilities necessary for modern desktop rendering:
- Bindless descriptor arrays
- Hardware ray tracing (acceleration structures)
- Explicit bind group heap allocation

Lyra's RHI extends the baseline to support these features natively while maintaining a consistent programming model across Vulkan, D3D12, and Metal.

### Resource Management & Synchronization

WebGPU specifies automatic resource tracking and implicit layout transitions within the implementation. I decided not to enforce driver-level tracking in the RHI for two reasons:

1. **User-level lifetime management**: Enforcing resource tracking in every backend duplicates significant bookkeeping logic. Instead, resources are managed using lightweight handles and user-level smart pointers with custom destructors.
2. **Automated barriers via Frame Graph**: Rather than requiring manual barrier placement or heavy runtime tracking, Lyra includes a built-in **Frame Graph** (`Lyra/Rendering/FrameGraph.h`). The Frame Graph analyzes pass dependencies, automatically inserts batched barriers (`resource_barrier`), and manages transient resource aliasing.

## Shading Language

WebGPU relies on WGSL, which deliberately restricts access to advanced binding models like bindless arrays and descriptor indexing.

Instead of WGSL, Lyra uses [Slang](https://shader-slang.org/) as its primary shading language. Slang compiles to SPIR-V, DXIL, and MSL, supports interfaces and generics, and uses a modular import syntax (`import lyra;`) rather than preprocessor includes.

For consistent descriptor layouts, shaders organize arguments using `ParameterBlock<T>`. To handle layout customization portably, Lyra provides a built-in `lyra` module defining unified attributes:

- `[[lyra::group(N)]]` / `[[lyra::set(N)]]`: Explicitly sets descriptor set or register space index `N` on a `ParameterBlock`, allowing out-of-order declarations.
- `[[lyra::binding(N)]]`: Explicitly assigns binding slot `N` within a struct or block.
- `[[lyra::binding(binding, set)]]`: Sets both binding slot and group index on standalone resources.
- `[[lyra::immediates]]`: Designates constant buffers passed as push/root constants.
- `[[lyra::dynamic]]`: Marks buffers with dynamic offsets.

The Slang compiler backend lowers these attributes into target-specific annotations (`[[vk::binding]]`, `: register(..., space...)`, Metal buffer slots) while emitting accurate reflection data for the pipeline layout.

### Layout Example

```hlsl
import lyra;

struct Camera
{
    float4x4 proj;
    float4x4 view;
};

struct Material
{
    [[lyra::binding(5)]]
    Texture2D<float4> albedo;

    [[lyra::binding(2)]]
    SamplerState smp;
};

// Declared out-of-order: material is explicitly group 2, scene is explicitly group 0
[[lyra::group(2)]]
ParameterBlock<Material> material;

[[lyra::group(0)]]
ParameterBlock<Camera> scene;

// Standalone resource with simultaneous binding slot and group index
[[lyra::binding(7, 3)]]
Texture2D<float4> env_lut;
```

In this setup:
- **Group 0**: Contains `scene` at binding slot 0.
- **Group 2**: Contains `material` with `albedo` at slot 5 and `smp` at slot 2.
- **Group 3**: Contains `env_lut` at slot 7.
- Target-specific register spaces and argument buffer bindings are generated automatically during compilation.

## Backend Discrepancies

Although modern graphics APIs share conceptual foundations, several differences arise during implementation:

1. Binding model and argument buffers
2. Immediates (push/root constants)
3. Dynamic uniforms
4. Vertex attribute buffer slots
5. Ray tracing acceleration structures

### Binding Model & Bindless

WebGPU and Vulkan use a descriptor set / group-based model where shader arguments are bound together by update frequency.

D3D12 manages descriptors through descriptor tables and direct root descriptors, offering a superset of Vulkan's model. Root descriptors require separate methods for graphics and compute (e.g., `SetGraphicsRootConstantBufferView` vs `SetComputeRootConstantBufferView`).

Metal supports direct resource binding per shader stage (`setVertexBuffer`, `setFragmentBuffer`) and indirect binding via **argument buffers**. When compiling with Slang, `ParameterBlock` translates directly into Metal argument buffers, which are also necessary for bindless support on Apple Silicon.

To represent bindings across backends, `GPUBindGroupLayoutEntry` includes an explicit count:
- `count = 1`: Standard single resource binding.
- `count > 1`: Fixed-size array of resources.
- `count = ~size_t(0)`: Unbounded bindless descriptor array.

To distinguish between direct and indirect binding modes on Metal, the layout uses `GPUBindingIndex`:

```cpp
struct GPUBindingIndex
{
    ushort index = 0;
    bool   from_argument_buffer = false;
};
```

### Immediates

Immediates pass small blocks of uniform data directly into shader registers without buffer allocations. They are intended for high-frequency parameters like model matrices or material indices.

Each backend lowers immediates differently:
- **Vulkan**: Push constants (`vkCmdPushConstants`) bound to `VK_SHADER_STAGE_ALL`.
- **D3D12**: 32-bit root constants (`SetGraphicsRoot32BitConstants` / `SetComputeRoot32BitConstants`) in reserved `space999`.
- **Metal**: Inline bytes (`setVertexBytes` / `setFragmentBytes` / `setBytes`) at reserved buffer slot 30.

In Slang, immediates are declared with `[[lyra::immediates]]` on a standalone `ConstantBuffer<T>`:

```hlsl
import lyra;

[[lyra::immediates]]
ConstantBuffer<MVP> mvp;
```

Reflection reports the required size via `GPUPipelineLayoutDescriptor::immediate_size`. Commands push immediate data using `command.set_immediates(offset, size, data)`.

### Dynamic Uniforms

Dynamic uniforms allow specifying a byte offset into a buffer when binding, avoiding descriptor updates when switching per-object offsets.

- **Vulkan**: Bound using dynamic descriptor offsets.
- **D3D12**: Implemented by binding GPU virtual addresses via root constant buffer views.
- **Metal**: Directly supported by passing an offset to buffer binding functions (`setVertexBuffer:offset:atIndex:`).

Because Metal argument buffers do not support offset rebinding within an existing indirect buffer, dynamic buffers must be declared outside `ParameterBlock`:

```hlsl
[[lyra::dynamic]]
ConstantBuffer<PerDrawData> per_draw;
```

### Vertex Attributes

Vulkan and D3D12 manage vertex attributes independently from descriptor bindings. In Metal, however, vertex buffers share the same buffer argument table as shader buffers.

Because bind groups start at slot 0 and immediates occupy slot 30, vertex buffers in the Metal backend are bound from the top of the table in descending order:
- `slot(immediates) - 1` (slot 29) for vertex buffer 0
- `slot(immediates) - 2` (slot 28) for vertex buffer 1, and so on.

### Ray Tracing

Lyra exposes hardware-accelerated ray tracing across Vulkan (VK_KHR_ray_tracing_pipeline), D3D12 (DirectX Raytracing DXR), and Metal (MPS / ray tracing primitives):

- **Bottom-Level Acceleration Structures (BLAS)**: Built from triangle or procedural geometry via `create_blas` and `GPUBlasDescriptor`.
- **Top-Level Acceleration Structures (TLAS)**: Aggregate BLAS instances and world transforms via `create_tlas` and `GPUTlasDescriptor`.
- **Shader Binding**: Shaders access acceleration structures through `GPUBVHBindingLayout` within standard bind groups.
- **Pipelines**: Ray tracing shaders (raygen, closest hit, miss) are linked into pipeline state objects via `create_raytracing_pipeline`.

## Frame Graph

To eliminate manual barrier management and simplify multi-pass rendering, Lyra uses a Frame Graph (`Lyra/Rendering/FrameGraph.h`).

The Frame Graph separates render pass configuration from execution:

1. **Setup Phase**: Passes declare resource reads (`READ`, `SAMPLE`, `PRESENT`) and writes (`WRITE`, `RENDER`). Resources are tracked using versioned handles (`FrameGraphHandle<T>`) supporting SSA semantics.
2. **Compilation**: The graph compiles passes into an execution DAG, culls unused passes, identifies resource lifetimes, and aliases transient buffers and textures.
3. **Execution Phase**: The graph executes active passes, automatically injecting batched barriers via `FrameGraphBarrierBatch` (`cmdlist.resource_barrier()`) before passing command encoders to pass lambdas.

### Frame Graph Pipeline Example

The following snippet demonstrates building a two-pass rendering pipeline: rendering geometry to an offscreen HDR texture, then sampling that texture to tone-map directly into the imported swapchain backbuffer:

```cpp
#include <Lyra/Rendering/FrameGraph.h>

using namespace lyra;

struct ScenePassData
{
    FrameGraph::Resource hdr_color;
};

struct PostPassData
{
    FrameGraph::Resource input_color;
    FrameGraph::Resource ldr_output;
};

void build_frame(FrameGraph::Builder& builder, const GPUSurfaceTexture& backbuffer, uint width, uint height)
{
    // Pass 1: Render scene geometry to offscreen HDR texture
    auto scene_data = builder.add_pass<ScenePassData>("scene-pass",
        [&](ScenePassData& data, FrameGraph::PassBuilder& pass) {
            GPUTextureDescriptor desc = {};
            desc.size.width           = width;
            desc.size.height          = height;
            desc.format               = GPUTextureFormat::RGBA16FLOAT;
            desc.usage                = GPUTextureUsage::RENDER_ATTACHMENT | GPUTextureUsage::TEXTURE_BINDING;

            // declare transient texture and mark as render target
            data.hdr_color = pass.render(pass.create<FrameGraph::Texture>(desc));
        },
        [](const ScenePassData& data, FrameGraph::Resources& resources, FrameGraphContext* ctx) {
            auto rt = resources.get<FrameGraph::Texture>(data.hdr_color);
            // draw scene into rt->view using ctx->cmdlist
        });

    // Pass 2: Post-process tone map to swapchain backbuffer
    builder.add_pass<PostPassData>("tonemap-pass",
        [&](PostPassData& data, FrameGraph::PassBuilder& pass) {
            FrameGraph::Texture imported_backbuffer{};
            imported_backbuffer.texture = backbuffer.texture;
            imported_backbuffer.view    = backbuffer.view;

            // sample offscreen HDR texture produced by pass 1
            data.input_color = pass.sample(scene_data.hdr_color);
            // write directly into imported swapchain surface
            data.ldr_output  = pass.render(pass.import(imported_backbuffer));
        },
        [](const PostPassData& data, FrameGraph::Resources& resources, FrameGraphContext* ctx) {
            auto src = resources.get<FrameGraph::Texture>(data.input_color);
            auto dst = resources.get<FrameGraph::Texture>(data.ldr_output);
            // execute tonemapping shader using ctx->cmdlist
        });
}
```
