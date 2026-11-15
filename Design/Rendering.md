# Rendering Architecture

The **Rendering Subsystem** (`Lyra/Rendering/`) bridges the scene representation (`Lyra/Scene/`), the asset management system (`Lyra/Assets/`), and the low-level Render Hardware Interface (`Lyra/Graphics/`). 

It is responsible for scene extraction, primitive culling, material property resolution, acceleration structure management, and driving multi-pass execution through the Frame Graph.

---

## 1. Scene Extraction & The Render World

Rendering in Lyra operates on a decoupled extraction model to isolate render pass recording from ECS hierarchy mutation and gameplay simulation.

```
[ECS World (SceneTree / Registry)]
           │
           ▼  RenderWorld::extract()
[RenderScene (Transient Primitives)]
   ├── RenderView (Cameras, Jitter, Projections)
   ├── RenderLights (Directional, Point, Spot)
   ├── Meshes (Indexed Draw Batches)
   ├── Meshlets (Cluster Dispatch Batches)
   ├── SDF Volumes (Analytic / CSG Primitives)
   └── BVH Instances (Ray Tracing TLAS Inputs)
           │
           ▼  FrameGraph Passes
[GPU Command Recording & Dispatch]
```

### Extraction Decoupling
- **`RenderWorld::extract()`**: Runs at the start of a frame. It queries active camera views, visible lights, transform hierarchies, and geometry caches, packaging them into a transient, linear `RenderScene`.
- **Zero Asset-Query Overhead in Passes**: Passes execute without synchronously querying `AssetServer`. All draw-essential data (matrices, vertex/index buffer slices, material factors, pipeline identifiers) are resolved during extraction and copied into contiguous arrays.

---

## 2. Geometric Primitives

Lyra supports multiple geometric presentation paths adapted to different rendering algorithms and hardware tiers:

| Primitive Category | Representation | Target Pipeline |
| :--- | :--- | :--- |
| `MeshPrimitive` | Traditional indexed triangle buffers (`IndexedGeometry`) | Rasterizer (Forward, Deferred GBuffer, Shadow Passes) |
| `MeshletPrimitive` | Cluster meshlet batches (`MeshletGeometry`) | Task / Mesh shader pipelines (GPUDriven) |
| `SDFPrimitive` | Analytic shapes (Sphere, Box, Capsule) & CSG operations | Raymarching compute passes |
| `BVHPrimitive` | Bottom-Level Acceleration Structure (`GPUBlasHandle`) | Hardware ray tracing pipelines (`GPUTlas`) |

---

## 3. Primitive Material Architecture

Extracted primitives carry immediate material parameters to support fast push-constant submission, GBuffer encoding, and procedural mesh rendering without requiring a full texture descriptor bind per draw.

### `struct PrimitiveMaterial`
Rather than encoding material factors into untyped vectors (e.g. `Vector3 material_params`), Lyra uses a dedicated, self-documenting, 16-byte-aligned struct:

```cpp
struct PrimitiveMaterial
{
    Vector4 base_color           = Vector4(1.0f);
    float   roughness            = 0.5f;
    float   metallic             = 0.0f;
    float   clear_coat           = 0.0f;
    float   alpha_cutoff         = 0.5f;

    // Fast-path parameters for non-standard / procedural shading models
    Vector4 custom_data          = Vector4(0.0f);

    // Offset into global custom material parameter storage buffer
    uint    custom_params_offset = 0;
};
```

### Architectural Benefits:
1. **Type Safety & Maintainability**: Eliminates indexing bugs where roughness and metallic channels might otherwise be swapped.
2. **GPU Memory Layout Alignment**: Fields naturally align to 16-byte boundaries, matching std140 / std430 uniform buffer and push constant layouts across Vulkan, D3D12, and Metal.
3. **Procedural Fallback**: Procedurally spawned geometries (`spawn_cube()`, `spawn_sphere()`) render immediately with valid PBR characteristics without allocating disk-backed material assets.

---

## 4. Custom Material & Pipeline Architecture

Modern scenes frequently require custom materials (water with wave parameters, car paint with clear-coat flakes, stylized toon shaders with discrete bands, or subsurface scattering). Storing arbitrary parameters directly on `RenderPrimitive` would inflate the struct size, degrading cache locality for standard meshes.

Lyra implements a **tiered 3-pillar strategy** for extensible custom material support:

### Pillar 1: Pipeline Segregation via `CustomPipelineID`
Custom materials requiring dedicated vertex manipulation, custom blend modes, or distinct fragment shaders compile into unique `GPURenderPipeline` instances.

To prevent raw integer misuse, the rendering subsystem wraps pipeline identifiers in a strongly typed struct:

```cpp
struct CustomPipelineID
{
    uint value = 0;

    [[nodiscard]] constexpr bool is_custom() const noexcept   { return value != 0; }
    [[nodiscard]] constexpr bool is_standard() const noexcept { return value == 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return is_custom(); }

    static constexpr CustomPipelineID standard() noexcept { return {0}; }

    auto operator<=>(const CustomPipelineID&) const = default;
};
```

- `CustomPipelineID::standard()` (`value == 0`) instructs render passes to use the default engine PBR/Unlit pipeline.
- Non-zero IDs identify registered custom pipelines. Passes bucket and sort draw calls by `CustomPipelineID` to minimize pipeline state switches.

### Pillar 2: Global GPU Parameter Storage (`custom_params_offset`)
For materials requiring arbitrary uniform data (matrices, extra texture indices, physics attributes):
1. **CPU Allocation**: The material's custom parameter struct is uploaded into a global GPU ByteAddress / Structured Buffer (`CustomMaterialBuffer`).
2. **Primitive Offset**: `PrimitiveMaterial::custom_params_offset` records the byte or element offset.
3. **Shader Unpacking**: Slang/HLSL shaders unpack their custom struct directly:
   ```slang
   struct WaterMaterialParams
   {
       float wave_amplitude;
       float wave_frequency;
       float3 absorption_color;
   };

   // Loaded inside custom material shader
   WaterMaterialParams water = custom_material_buffer.Load<WaterMaterialParams>(material.custom_params_offset);
   ```

### Pillar 3: Fast-Path Inline Payload (`custom_data`)
For simple custom materials requiring only a few extra scalars (e.g. toon shading band count, dissolve cutoff, emissive pulsation rate), `PrimitiveMaterial::custom_data` (`Vector4`) passes values directly via push constants / immediates, bypassing global buffer fetches.

---

## 5. Render Pass Execution & Pass Bucketing

Render passes query `RenderScene` and group primitives into sorted draw buckets:

1. **State Sorting**: Primitives are sorted by:
   $$\text{SortKey} = (\text{BlendMode} \ll 32) \mid (\text{CustomPipelineID} \ll 16) \mid \text{MaterialID}$$
2. **Batch Emission**:
   - Primitives sharing standard pipelines execute via batched instancing or direct push-constant submission.
   - Primitives bound to `CustomPipelineID` trigger pipeline rebinds and bind custom parameter blocks.
3. **Frame Graph Integration**: Transient attachments (GBuffer targets, shadow maps, ambient occlusion textures) are tracked with automated resource barriers and memory aliasing.
