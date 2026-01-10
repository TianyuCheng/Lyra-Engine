# RHI (Render Hardware Interface)

## API Design

As a rendering engine, RHI sets the foundation stone.

**Lyra-Engine** plans to support different graphics APIs running on different platforms,
therefore a comprehensive understanding of different graphics APIs is necessary to design
the rendering API.

In the past I have tried to come up with some abstraction layers for Vulkan. They work
well for my Vulkan-only backend, but there is very little / close to no guarantee that
they will work with other graphics APIs. To make sure the RHI abstraction works for most
of the graphics backends, I picked API from [WebGPU](https://www.w3.org/TR/webgpu/) as
a common subset among all of the features provided by different APIs. Of course WebGPU
is not perfect because it still lacks some fundamental features that I will need for
developing my use cases, for example, bindless textures, ray tracing support, etc.
However, it is a good enough starting point.

After spending some time researching on WebGPU, I found WebGPU is similar to a simplified
version of Vulkan, where its abstraction is largely based on Vulkan, including binding
model, push constants, dynamic uniforms, etc. What's primarily being simplified is the
resource tracking (lifetime, layout transition). However, I do not plan to support either
in my RHI backend because of the following reasons:

1. Resource tracking could be a user level implementation, implementing it in API level
means the same logic repeats in every backend. While in user level, we can simply use
smart pointers with custom destructor to manage most of the cases. Therefore I don't
want to enforce automatic resource lifetime tracking here.

2. Modern graphics API is moving towards providing user more control to maximize throughput.
Although layout transition has been tedious, graphics engine have already adopted frameworks
like [Frame Graph](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in)
to automatically manage resource transitions/barriers. I am planning to do the same.

## Shading Language

WebGPU has introduced its own shading language (WGSL). Without deep analysis on why they do so,
my guess is that they introduced their own shading language to limit the features they support,
especially on the binding model (see next section). When WebGPU controls the shading language,
they can customly generate different binding if implemented with different backends.

In this engine, I have made the decision not to use WGSL. The biggest reason being that
WebGPU and WGSL does not support a lot of the features that I will need, namely bindless descriptors.
WGSL has no native support for binding an array of resources either. If I need to bind an array
of textures, I have have to manually bind them in different binding slots. I believe WebGPU
will eventually include bindless, it will probably be a long time.
There is an open [issue](https://github.com/gpuweb/gpuweb/issues/380) on gpuweb github for a long
time (since 2019).

Therefore I am falling back to another shader language: [slang](https://shader-slang.org/).
Slang is a relatively new shader language that supports compiling to different targets:
SPIRV, DXIL, MSL, etc. It also has some new features like differential shader compiling.
These are features that I currently don't use, but still good to have.

Slang is module based, rather than preprocessor based. This is potentially a good feature
that can potentially save compilation time. Slang also includes good support for
[interfaces and generics](https://shader-slang.org/slang/user-guide/interfaces-generics.html),
making it a good candidate for modern shader development.

Finally, slang uses a deterministic rule for automatic binding deduction. For maximum compatibility,
I would also encourage users to exclusively use `ParameterBlock` for better shader binding organization,
except for a few cases (also detailed in the next section).

## Backend Differences

This [post](https://alain.xyz/blog/comparison-of-modern-graphics-apis) from Alain Galvan
demonstrated the commonalities among Vulkan, D3D12 and Metal APIs.q Although being a :well
written article, it does give me an illusion that all three APIs are similar enough.

My personal journey of implementing the backends found that there are several discrepancies:
1. binding model
3. push constant
4. dynamic uniforms
5. vertex attributes

### Binding Model

WebGPU API uses a similar binding model with Vulkan. The binding model adopts a group based
binding design, where shader arguments are organized by groups. Creating/updating the binding
group must be conducted at the scope of a group. Individual bindings cannot be bound separately.
This design encourages that developers to take shader argument update frequency into consideration.
Arguments with the same update frequency should be grouped together. Since the design is exactly
the same as Vulkan, implementing the WebGPU abstraction is not a problem.

D3D12 offers a more flexible descriptor system, where descriptors can be both managed by groups,
or individually. D3D12's binding model is a super set of Vulkan's binding model. When managed by
groups, D3D12 offers `DescriptorTable`; when managed separately, D3D12 also offers
`SetGraphicsRootConstantBufferview`, `SetGraphicsRootShaderResourceView`, etc. Implementing a
the WebGPU abstraction using a subset is also not a problem. However, the caveat is that D3D12
uses different functions for graphics and compute, for example: `SetGraphicsRootConstantBufferView`
vs `SetComputeRootConstantBufferView`.

Similar to D3D12, Metal also offers directly binding resources within/without a binding group.
Metal goes even further than D3D12 for directly binding resources to a specific shader stage.
For example, `SetVertexBuffer`, `SetFragmentBuffer` and `SetBuffer` (for compute). Since we
record the shader stage visibility in the bind group layout, we know easily which functions
to call, but this also introduces more tedious work. Metal offers argument buffer for indirectly
binding resources in group. This is an inherently different approach from directly binding resources.
Although I am trying to stay away from building two different code paths for binding resources,
it is still inevitable because my choice of shading language (slang) will automatically translates
`ParamaterBlock` to argument buffer. Additionally, argument buffer is also the only way to achieve
bindless resources on Metal. While it is feasible to implement the WebGPU abstraction using Metal,
it is quite an odyssey.

Without searching for further evidence, but my feeling is that Vulkan's binding model is inherently
less comprehensive, therefore Vulkan introduced additional ad-hoc features like push constant and
dynamic uniform to provide user with tools that enhance performance.

### Push Constant

Push constant is a small feature that Vulkan introduced. Push constant allows setting a number of
bytes directly with shader invocation. Push constant is more lightweight than bindings, and widely
used for most frequently changing parameters, for example, material id.

While it is a Vulkan only concept, other two backends also have alternatives to implement the same
idea. D3D12 offers `SetComputeRoot32BitConstant` and `SetGraphicsRoot32BitConstant`. Metal directly
offers `setBytes` to implement the same idea. However, there is still a problem for mimicing the
behavior. WebGPU/Vulkan push constants do not have an explicit group/binding index, but in both
D3D12/Metal, all resources requires a separate binding slot. In D3D12, we explicitly reserve space
999 for push constants. Metal only supports 30 buffers for direct binding, therefore we reserve
slot 30 for push constant.

In shader slang, push constant must be explicitly annotated, and unfortunately due to different
binding models, we must annotate the push constant for all 3 backends. We use macros to predefine
the binding annotation for push constant. Here's an example:

```hlsl
[[vk::push_constant]]
ConstantBuffer<MVP> mvp : PUSH_CONSTANT;
```

In the above example, `[[vk::push_constant]]` is an explicit requirement for Vulkan backend, while
the macro `PUSH_CONSTANT` is a macro automatically defined based on the backend type.
For it to work across all backends, it needs to be bound directly without `ParameterBlock`.
This is an exception that every binding should be bound in a `ParameterBlock`.

### Dynamic Uniform

Dynamic uniform is also a feature that Vulkan introduced. This allows users to specify an offset
into buffers for binding. Under the hood, the whole descriptor set is not re-bound, it is just the
pointer to the buffer is updated, saving some amount of graphics driver effort.

As I mentioned in the binding model section, D3D12 supports `SetGraphicsRootConstantBufferview` which
allows users to specify the buffer address, so it naturally supports uniforms with dynamic offset.
However, there is a caveat during implementation. That is, `SetGraphicsRootConstantBufferview` only
allows resources to be bound at root signature, rather than under descriptor table. This unsurprisingly
breaks a hole in the our binding model implementation. If a binding is marked as dynamic uniform, it
must be moved out from the descriptor table.

Metal directly supports binding a buffer with offset (since it is not inherently exposing the GPU address
of a buffer). However, this API only support directly binding MTLBuffer. Therefore it is in the same
situation as D3D12. Furthermore, when bound in a group, the argument buffer implementation does not offer
the flexibility to take a binding out and bound separately, hence this feature is not supported if buffer
is placed under a `ParameterBlock`.

Since dynamic uniform is not something shown from the shader, shader reflection does not know which
resource binding is dynamic. Therefore, I introduced a custom user attributes `[[lyra::dynamic]]`.

```hlsl
[lyra::dynamic]
ConstantBuffer<MVP> mvp;
```

For it to work across all backends, it needs to be bound directly without `ParameterBlock`.
This is also an exception that every binding should be bound in a `ParameterBlock`.

### Vertex Attributes

Vertex attribute is mostly the same in all 3 backends. However, Metal treats vertex buffers in the same
way as other regular buffers. Hence, regular vertex attributes are also consuming the regular buffer slots
in Metal backend. As we are enforcing a determinstic group/binding resolution rule, our regular bind groups
would start from buffer slot 0. If we choose to bind vertex attributes starting from buffer slot 0, it will
definitely collide with other buffer resources. As a workaround, our implementation chooses to bind the vertex
attributes from the top buffer slots in a descending order. We have already reserved the top slot for push
constants, therefore we can only start binding vertex attributes from `slot(push constants) - 1`. The next
vertex attribute would be bound from `slot(push constants) - 2`.
