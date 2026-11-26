# Milestone 1: Core Renderer Maturity & Multi-Backend Parity

**Goal**: Solidify the primary rasterization pipelines (Forward+ and Deferred Hybrid), establish declarative pipeline serialization with binary PSO caching, implement offline Slang shader pre-compilation, and ensure perceptual visual parity across Vulkan, SoftGPU, and D3D12.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 1.4 Multi-Backend RHI Parity
- [ ] **1.4.1 Vulkan & SoftGPU Visual Parity**
  - Run `just test rhi-tests` comparing frame captures between hardware Vulkan and CPU SoftGPU (AVX2 rasterizer).
- [ ] **1.4.2 D3D12 Backend Parity Audit**
  - Directory: [`Modules/Graphics/D3D12/`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Graphics/D3D12)
  - Verify root signatures, descriptor heaps, and pipeline state objects correctly handle Slang DXIL bytecode; run test fixtures with `--gfxapi d3d12`.

---

## 1.5 Declarative Pipeline Serialization, Deserialization & Binary PSO Caching
- [ ] **1.5.1 Declarative Pipeline Asset Format (`.pipeline`) & JSON/TOML Schema**
  - Files: `Engine/Include/Lyra/Assets/Format/PipelineAsset.h`, `Engine/Source/Assets/Format/PipelineAsset.cpp`
  - Module: `Modules/Assets/Pipeline/PipelineCookerPlugin.cpp`
  - Declarative schema capturing `GPURenderPipelineDescriptor` and `GPUComputePipelineDescriptor` in human-readable and cooked binary forms:
    - **Shader Stage Definitions**: Module virtual path (`sys://Shaders/...`), entry points (`vsmain`, `fsmain`, `csmain`), and compile-time macro definitions.
    - **Rasterizer State**: Primitive topology (`TRIANGLE_LIST`), polygon fill mode, cull mode (`NONE`, `BACK`, `FRONT`), front face (`CCW`), and depth bias parameters.
    - **Depth / Stencil State**: Depth write toggle, comparison operator (`GREATER_EQUAL` for reverse-Z), stencil front/back test and operations.
    - **Color Attachment & Blend States**: Target format specifications (`GPUTextureFormat`), color write masks, and predefined blend modes (`Opaque`, `Additive`, `AlphaBlend`, `PremultipliedAlpha`).
  - Eliminates 50–100 lines of procedural, repetitive C++ initialization code in every render and compute pass.
- [ ] **1.5.2 Automated Shader Reflection & Layout Deserialization**
  - Files: `Engine/Include/Lyra/Rendering/PipelineManager.h`, `Engine/Source/Rendering/PipelineManager.cpp`
  - Automatically derive `GPUPipelineLayoutDescriptor` and `GPUBindGroupLayoutDescriptor` sets directly from Slang reflection metadata:
    - Automatically synthesize bind group slots, binding types (uniform buffer, storage buffer, sampled texture, storage texture, sampler), and visibility masks.
    - Extract and set immediate / push constant size directly from Slang reflection, eliminating manual boilerplate bind group wiring in passes.
- [ ] **1.5.3 Pass Initialization Refactoring & Fluent Pipeline Builder API**
  - Refactor all builtin passes (`GBufferPass`, `ForwardPass`, `DepthPrepass`, `ShadowMapPass`, `SkyboxPass`, `OceanPass`, `VolumetricCloudPass`, `AutoExposurePass`, `BloomPass`, `DepthOfFieldPass`, `FXAAPass`, `TonemappingPass`, `HiZPyramidPass`, `LightCullingPass`) to instantiate pipelines declaratively:
    ```cpp
    // replace manual module/layout/state wiring with single declarative load or fluent builder
    pipeline = pipeline_manager->load_render_pipeline("sys://Pipelines/PostProcess/FXAA.pipeline");
    ```
  - Centralize pipeline lifecycle management: automatic handle cleanup, deduplication of identical PSOs, and automatic hot-reload propagation on shader modification.
- [ ] **1.5.4 Multi-Backend Binary PSO Disk Caching (`VkPipelineCache` / D3D12 Cached PSO / Metal Binary Archives)**
  - Files: `Modules/Graphics/Vulkan/VkPipelineCache.cpp`, `Modules/Graphics/D3D12/D3D12PipelineCache.cpp`, `Modules/Graphics/Metal/MtlPipelineCache.cpp`
  - Compute 64-bit deterministic hash of shader SPIR-V/DXIL/MSL bytecode + pipeline state descriptor + GPU hardware driver version.
  - Persist compiled driver binary blobs to `.cache/pipelines/` on disk:
    - Vulkan: Save/load `VkPipelineCache` via `vkGetPipelineCacheData`.
    - D3D12: Serialize `ID3D12PipelineLibrary` or `D3D12_CACHED_PIPELINE_STATE`.
    - Metal: `MTLBinaryArchive` serialization.
  - Completely eliminates runtime compilation stutter, frame drops, and application cold start latency.
- [ ] **1.5.5 Pipeline Serialization & Binary Cache Unit Tests**
  - Directory: `Tests/pipeline_serialization/`
  - Automated unit test verifying round-trip JSON/binary serialization, deserialization into valid `GPURenderPipeline` and `GPUComputePipeline`, cache hits on disk, and cache invalidation when shader source or macros change.
  - Command: `just test pipeline_serialization`.

---

## 1.6 Offline Shader Pre-Compilation, Content-Addressable Caching & Permutations
- [ ] **1.6.1 Pre-Compiled Binary Shader Asset Format (`.shader` / `.shaderbin`)**
  - Files: `Engine/Include/Lyra/Assets/Format/ShaderAsset.h`, `Engine/Source/Assets/Format/ShaderAsset.cpp`
  - Module: `Modules/Assets/Shader/ShaderCookerPlugin.cpp`
  - Cooker asset format packaging pre-compiled bytecode and serialized reflection:
    - **Header**: Magic (`0x53484452` / `'SHDR'`), format version, and target backend (`SPIRV`, `DXIL`, `MSL`, `HOST`).
    - **Chunk `CHUNK_CODE`**: Compressed target bytecode blobs per entry point (`vsmain`, `fsmain`, `csmain`, `raygen_main`, `closesthit_main`, `miss_main`).
    - **Chunk `CHUNK_REFL`**: Serialized `ShaderReflection` data (bind group layouts, resource bindings, push constant / immediate sizes, sampler descriptors).
    - **Chunk `CHUNK_HASH`**: 128-bit source text + include dependency hash for automatic cache validation.
  - Enables standalone game players and distribution builds to load pre-compiled shaders instantly without linking or shipping the Slang compiler runtime.
- [ ] **1.6.2 Content-Addressable Two-Tier Shader Disk Cache**
  - Files: `Modules/Compiler/Slang/ShaderCache.h`, `Modules/Compiler/Slang/ShaderCache.cpp`
  - Compute canonical 128-bit hash (Murmur3 / xxHash128) over:
    - Full recursive source dependency tree (resolving all Slang `import` and `#include` files via VFS).
    - Target backend (`GPUBackend::VULKAN`, `D3D12`, `METAL`, `SOFTGPU`).
    - Preprocessor macro definitions (`KEY=VALUE` pairs sorted lexicographically).
    - Compiler build version & optimization flags.
  - Two-tier storage:
    - **L1 In-Memory Cache**: Thread-safe hash table storing compiled shader blobs for instantaneous in-frame lookups.
    - **L2 Persistent Disk Cache**: Binary storage under `.cache/shaders/<hash>.bin` (or `cache://shaders/`), providing zero compilation overhead across application restarts.
- [ ] **1.6.3 Shader Permutation & Variant Pruning System**
  - Files: `Engine/Include/Lyra/Compiler/ShaderPermutation.h`, `Engine/Source/Compiler/ShaderPermutation.cpp`
  - Support declarative multi-compile directives (e.g. `HAS_NORMAL_MAP=0/1`, `SHADOW_PCF_SAMPLES=4/16`, `SKINNED=0/1`, `ATMOSPHERE_IBL=0/1`).
  - Prune invalid / impossible variant combinations via boolean constraint expressions (e.g. `SKINNED` implies `HAS_TANGENTS`), preventing combinatorial $2^N$ explosion.
  - Generate compact sparse permutation indices stored inside `.shader` asset chunks.
- [ ] **1.6.4 Multi-Threaded Parallel Shader Compilation via Job System**
  - Files: `Modules/Compiler/Slang/SlangCompiler.cpp`, `Engine/Source/Compiler/AsyncShaderCompiler.cpp`
  - Map compilation requests onto `JobScheduler`, fanning out independent shader compilation jobs across all CPU worker threads during asset cooking or editor boot.
  - Asynchronous background compilation with progress tracking: non-blocking pipeline creation with fallback placeholder shaders in the editor viewport.
  - Add recipe `just compile-shaders [target]` to CLI tooling for batch pre-compiling all project shaders in parallel.
- [ ] **1.6.5 Shader Caching & Pre-Compilation Unit Tests**
  - Directory: `Tests/shader_cache/`
  - Test suite verifying:
    - L1/L2 cache hit rate on identical compilation descriptors.
    - Automatic cache invalidation when included `.slang` header or macro definitions change.
    - Cooked `.shader` binary loading parity against runtime Slang JIT compilation.
  - Command: `just test shader_cache`.

---

## 1.7 Per-Renderer TOML Configuration Architecture & Quality Presets
- [ ] **1.7.1 Renderer Configuration Schema & TOML Serialization (`IRendererConfig`)**
  - Files: `Engine/Include/Lyra/Rendering/RendererConfig.h`, `Engine/Source/Rendering/RendererConfig.cpp`
  - Replace monolithic `RendererSettings` struct with modular, typed per-renderer configuration models serialized via `lyra::TOML` / `lyra::JSON`:
    - `ForwardConfig`: Clustered light grid depth slices, max lights per cluster, MSAA sample count, depth prepass toggle.
    - `DeferredConfig`: CSM split lambda ($\lambda = 0.85$), PCF filter kernel radius, tile culling parameters, Split-Sum IBL reflection toggle, emissive bloom threshold.
    - `PathTracerConfig`: Max path bounces, Russian roulette minimum cutoff probability, VNDF specular sampling toggle, SPP per frame, ReSTIR candidate count, spatial reuse radius, À-Trous denoiser parameters ($\sigma_z, \sigma_n, \sigma_l$, iterations).
    - `GPUDrivenConfig`: Frustum culling enable, Hi-Z occlusion culling toggle, backface cone culling toggle, two-phase occlusion batch size.
- [ ] **1.7.2 RendererManager Configuration Lifecycle & Preset Loader**
  - Files: `Engine/Include/Lyra/Rendering/RendererManager.h`, `Engine/Source/Rendering/RendererManager.cpp`
  - Load and validate default renderer configuration from `sys://Config/Renderers/<RendererName>.toml`.
  - Quality Profile Engine: Support named presets (`Low`, `Medium`, `High`, `Ultra`, `Cinematic`) switching resolution scaling, shadow filter quality, bounce limits, and denoiser passes dynamically at runtime.
  - Hot-reload active renderer settings on file change notification via `AMSWatcher`.
- [ ] **1.7.3 Deterministic Headless Test Configuration Support**
  - Extend test runner (`runtest.py` and fixture `main.cpp`) to accept `--config <path_to_config.toml>` flag, ensuring deterministic, reproducible renderer parameters across local environments and CI runners.

---

## 1.8 Bindless Resource Model & Unified Descriptor Indexing Heap
- [ ] **1.8.1 Bindless Descriptor Heap Architecture (`VK_EXT_descriptor_indexing` / D3D12 SM6.6 / Metal Argument Buffers)**
  - Files: `Engine/Include/Lyra/Graphics/GPUBindlessHeap.h`, `Modules/Graphics/Vulkan/VkBindlessHeap.cpp`, `Modules/Graphics/D3D12/D3D12BindlessHeap.cpp`, `Modules/Graphics/Metal/MtlBindlessHeap.cpp`
  - Allocate global unbounded descriptor arrays on device initialization (e.g. 500,000 textures, 50,000 storage buffers, 1,000 samplers).
  - Strongly typed generational handles: `TextureBindlessHandle`, `BufferBindlessHandle` exposing 32-bit slot indices.
  - Slang shader interface:
    ```slang
    import lyra;
    [[lyra::bindless]] Texture2D<float4> g_textures[];
    [[lyra::bindless]] ByteAddressBuffer g_buffers[];
    [[lyra::bindless]] SamplerState      g_samplers[];
    ```
- [ ] **1.8.2 Geometry & Material Pipeline Migration to Bindless Indexing**
  - Files: `Engine/Source/Rendering/RenderWorld.cpp`, `Engine/Source/Rendering/GeometryBindGroupCache.cpp`
  - Replace per-draw `GPUBindGroup` allocation and switching with uniform push constant material/mesh index passing (`DrawPushConstants { uint material_id; uint mesh_id; }`).
  - Eliminates descriptor set churn, reduces CPU driver overhead by up to 80%, and acts as the fundamental bridge for GPU-Driven Meshlets (Milestone 4) and ReSTIR DI/GI (Milestone 2).

---

## 1.9 FrameGraph Transient VRAM Memory Aliasing
- [ ] **1.9.1 Virtual Allocation Lifetime Analysis & Memory Placement Heaps**
  - Files: `Engine/Include/Lyra/Rendering/FrameGraphMemory.h`, `Engine/Source/Rendering/FrameGraph.cpp`
  - Analyze pass dependency DAG and calculate disjoint lifetime intervals $[t_{\text{start}}, t_{\text{end}}]$ for all transient textures and buffers in the frame.
  - Group non-overlapping resources into shared memory blocks backed by placed allocations on `VkDeviceMemory` / `ID3D12Heap` / `MTLHeap`.
  - Handle alignment requirements (e.g. 64KB for standard textures, 4MB for MSAA / depth-stencil targets).
- [ ] **1.9.2 Aliasing Hazard Tracking & Barrier Insertion**
  - Ensure memory aliasing transitions between previous owner and new owner insert proper execution barriers (`VK_IMAGE_LAYOUT_UNDEFINED`, discard flags, aliasing barriers in D3D12).
  - Reduces total FrameGraph transient VRAM footprint by 40–60% without visual or functional degradation.

---

## 1.10 Async Compute Queue Scheduling & Cross-Queue Synchronization
- [ ] **1.10.1 Multi-Queue FrameGraph Scheduling (`GPUQueueType::COMPUTE`)**
  - Files: `Engine/Source/Rendering/FrameGraph.cpp`, `Modules/Graphics/Vulkan/VkQueue.cpp`, `Modules/Graphics/D3D12/D3D12Queue.cpp`
  - Detect compute passes with no direct raster dependencies (e.g. Clustered Light Culling, Hi-Z Pyramid downsample, ReSTIR candidate generation).
  - Dispatch passes on dedicated hardware asynchronous compute queues concurrently while the graphics queue executes independent workloads (Shadow Maps or Depth Prepass).
- [ ] **1.10.2 Cross-Queue Timeline Semaphore & Barrier Management**
  - Automatically insert timeline semaphore wait/signal operations and cross-queue resource ownership transfer barriers.
  - Yields 10–25% faster frame rendering times on modern multi-queue GPUs.

---

## 1.11 GPU Crash Diagnostics & Breadcrumb Tracking (DRED / Aftermath)
- [ ] **1.11.1 Device-Loss Breadcrumb Ring Buffer**
  - Files: `Engine/Include/Lyra/Graphics/GPUBreadcrumb.h`, `Modules/Graphics/Vulkan/VkBreadcrumb.cpp`, `Modules/Graphics/D3D12/D3D12Breadcrumb.cpp`
  - Allocate a persistent host-visible GPU ring buffer to record execution markers:
    - Pass begin / end tokens with pass name hash.
    - Draw call / compute dispatch sequence index.
    - Active pipeline and shader virtual path.
- [ ] **1.11.2 Crash Dump & Automatic Root-Cause Analyzer**
  - On `VK_ERROR_DEVICE_LOST` or `DXGI_ERROR_DEVICE_REMOVED`, read back breadcrumb buffer to identify the exact pass, draw/dispatch call, and Slang shader module that caused the GPU hang or TDR.
  - Log detailed crash diagnostics to `crash_gpu_<timestamp>.log` and trigger graceful debugger break.
