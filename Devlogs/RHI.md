# RHI (Render Hardware Interface)

## API Design

For a rendering engine, the RHI (Render Hardware Interface) serves as the foundation.

**Lyra-Engine** plans to support different graphics APIs running on different platforms,
therefore a comprehensive understanding of different graphics APIs is necessary to design
the rendering API.

In the past, I tried to design abstraction layers for Vulkan. They worked well for my
Vulkan-only backend, but there was little to no guarantee that they would work with
other graphics APIs. To ensure the RHI abstraction works for most graphics backends,
I selected the [WebGPU](https://www.w3.org/TR/webgpu/) API as a common subset of features
provided by the different APIs. Of course, WebGPU is not perfect because it still lacks
some fundamental features that I will need for developing my use cases, for example,
bindless textures, ray tracing support, etc. However, it is a good enough starting point.

After spending some time researching WebGPU, I found WebGPU to be similar to a simplified
version of Vulkan; its abstraction is largely based on Vulkan, including the binding model,
push constants, dynamic uniforms, etc. What is primarily simplified is resource tracking
(lifetime, layout transition). However, I do not plan to support either in my RHI backend
for the following reasons:

1. Resource tracking can be implemented at the user level; implementing it at the API level
means repeating the same logic in every backend. At the user level, we can simply use smart
pointers with custom destructors to manage most cases. Therefore I don't want to enforce
automatic resource lifetime tracking here.

2. Modern graphics APIs are moving towards providing users with more control to maximize throughput.
Although layout transition has been tedious, graphics engines have already adopted frameworks
like [Frame Graph](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in)
to automatically manage resource transitions/barriers. I am planning to do the same.

## Shading Language

WebGPU introduced its own shading language, WGSL. Without deep analysis, my guess is that
they did so to limit supported features, especially regarding the binding model (see next section).
By controlling the shading language, WebGPU can custom-generate different bindings when implemented
with different backends.

I have decided not to use WGSL for this engine. The main reason is that WebGPU and WGSL do not
support many features I need, such as bindless descriptors. WGSL has no native support for binding
an array of resources either. If I need to bind an array of textures, I have to manually bind them
to different binding slots. I believe WebGPU will eventually include bindless support, but it will
probably take a long time. There has been an open [issue](https://github.com/gpuweb/gpuweb/issues/380)
on the gpuweb GitHub for a long time (since 2019).

Therefore, I am falling back to another shader language: [Slang](https://shader-slang.org/).
Slang is a relatively new shader language that supports compilation to different targets: SPIR-V, DXIL,
MSL, etc. It also has some new features like differential shader compiling. These are features that I
currently don't use, but they are still good to have.

Slang is module-based rather than preprocessor-based. This is potentially a good feature
that can save compilation time. Slang also includes good support for
[interfaces and generics](https://shader-slang.org/slang/user-guide/interfaces-generics.html),
making it a good candidate for modern shader development.

Finally, Slang uses a deterministic rule for automatic binding deduction. For maximum compatibility,
I would also encourage users to exclusively use `ParameterBlock` for better shader binding organization,
except for a few cases (also detailed in the next section).

## Backend Differences

This [post](https://alain.xyz/blog/comparison-of-modern-graphics-apis) from Alain Galvan
demonstrated the commonalities among Vulkan, D3D12 and Metal APIs. Although it is a well-written
article, it gave me the illusion that all three APIs are similar enough.

My personal journey of implementing the backends revealed several discrepancies:
1. binding model
3. push constant
4. dynamic uniforms
5. vertex attributes

### Binding Model

The WebGPU API uses a binding model similar to Vulkan's. The binding model adopts a group-based
binding design, where shader arguments are organized by groups. Creating/updating the binding
group must be conducted at the scope of a group. Individual bindings cannot be bound separately.
This design encourages developers to take shader argument update frequency into consideration.
Arguments with the same update frequency should be grouped together. Since the design is exactly
the same as Vulkan, implementing the WebGPU abstraction is not a problem.

D3D12 offers a more flexible descriptor system, where descriptors can be both managed by groups,
or individually. D3D12's binding model is a superset of Vulkan's. When managed by groups, D3D12
offers `DescriptorTable`; when managed separately, D3D12 also offers `SetGraphicsRootConstantBufferview`,
`SetGraphicsRootShaderResourceView`, etc. Implementing the WebGPU abstraction using a subset is also
not a problem. However, the caveat is that D3D12 uses different functions for graphics and compute,
for example: `SetGraphicsRootConstantBufferView` vs `SetComputeRootConstantBufferView`.

Similar to D3D12, Metal also offers direct resource binding, with or without a binding group. Metal
goes even further than D3D12 by allowing resources to be bound directly to a specific shader stage.
For example, `SetVertexBuffer`, `SetFragmentBuffer` and `SetBuffer` (for compute). Since we record
the shader stage visibility in the bind group layout, we know easily which functions to call, but
this also introduces more tedious work. Metal offers argument buffers for indirectly binding resources
in a group. This is an inherently different approach from directly binding resources. Although I try
to avoid building two different code paths for binding resources, it is inevitable because my choice
of shading language (Slang) automatically translates `ParameterBlock` to argument buffers. Additionally,
argument buffers are also the only way to achieve bindless resources on Metal. While implementing the
WebGPU abstraction using Metal is feasible, it is quite an odyssey.

Without searching for further evidence, I feel that Vulkan's binding model is inherently less comprehensive;
therefore, Vulkan introduced additional ad-hoc features like push constants and dynamic uniforms to provide
users with tools that enhance performance.

### Push Constant

Push constants are a small feature that Vulkan introduced. Push constants allow setting a number of
bytes directly with a shader invocation. Push constants are more lightweight than bindings, and widely
used for most frequently changing parameters, for example, material id.

While it is a Vulkan-only concept, the other two backends have alternatives to implement the same idea.
D3D12 offers `SetComputeRoot32BitConstant` and `SetGraphicsRoot32BitConstant`. Metal directly offers
`setBytes` to implement the same idea. However, there is still a problem with mimicking the behavior.
WebGPU/Vulkan push constants do not have an explicit group/binding index, but in both D3D12/Metal,
all resources require a separate binding slot. In D3D12, we explicitly reserve space 999 for push constants.
Metal only supports 30 buffers for direct binding, so we reserve slot 30 for push constants.

In Slang, push constants must be explicitly annotated, and unfortunately, due to different binding models,
we must annotate them for all three backends. We use macros to predefine the binding annotation for push
constant. Here's an example:

```hlsl
[[vk::push_constant]]
ConstantBuffer<MVP> mvp : PUSH_CONSTANT;
```

In the above example, `[[vk::push_constant]]` is an explicit requirement for the Vulkan backend, while
the macro `PUSH_CONSTANT` is a macro automatically defined based on the backend type. For this to work
across all backends, it must be bound directly without a `ParameterBlock`. This is an exception to the
rule that every binding should be bound in a `ParameterBlock`.

### Dynamic Uniform

Dynamic uniforms are also a feature that Vulkan introduced. They allow users to specify an offset
into buffers for binding. Under the hood, the entire descriptor set is not re-bound; only the buffer
pointer is updated, saving some graphics driver overhead.

As I mentioned in the binding model section, D3D12 supports `SetGraphicsRootConstantBufferview`, which
allows users to specify the buffer address, naturally supporting uniforms with dynamic offsets. However,
there is a caveat during implementation. That is, `SetGraphicsRootConstantBufferview` only allows resources
to be bound at root signature, rather than under descriptor table. This unsurprisingly pokes a hole in our
binding model implementation. If a binding is marked as dynamic uniform, it must be moved out from the
descriptor table.

Metal directly supports binding a buffer with offset (since it is not inherently exposing the GPU address
of a buffer). However, this API only supports directly binding `MTLBuffer`. Therefore, it is in the same
situation as D3D12. Furthermore, when bound in a group, the argument buffer implementation does not offer
the flexibility to take a binding out and bind it separately, hence this feature is not supported if the
buffer is placed under a `ParameterBlock`.

Since dynamic uniforms are not explicit in the shader, shader reflection does not know which
resource binding is dynamic. Therefore, I introduced a custom user attribute: `[[lyra::dynamic]]`.

```hlsl
[lyra::dynamic]
ConstantBuffer<MVP> mvp;
```

For this to work across all backends, it must be bound directly without a `ParameterBlock`.
This is also an exception to the rule that every binding should be bound in a `ParameterBlock`.

### Vertex Attributes

Vertex attributes are mostly the same in all three backends. However, Metal treats vertex buffers the same
way as other regular buffers. Hence, regular vertex attributes also consume regular buffer slots in the
Metal backend. As we enforce a deterministic group/binding resolution rule, our regular bind groups start
from buffer slot 0. If we chose to bind vertex attributes starting from buffer slot 0, they would collide
with other buffer resources. As a workaround, our implementation binds vertex attributes from the top buffer
slots in descending order. We have already reserved the top slot for push constants, therefore we can only
start binding vertex attributes from `slot(push constants) - 1`. The next vertex attribute would be bound
from `slot(push constants) - 2`.
