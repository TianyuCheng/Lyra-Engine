# Milestone 4: Modern Geometry & GPU-Driven Meshlet Pipelines

**Goal**: Unlock high-density geometry rendering, cluster culling, and meshlet pipelines using industry-standard libraries (`meshoptimizer`, `mikktspace`) and custom task/mesh shader passes.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 4.1 Meshletization & Tangent Space Generation (Asset Cooking Pipeline)
- [ ] **4.1.1 MikkTSpace Pixel-Exact Tangent Space Generation**
  - Library: `mikktspace` (Morten Mikkelsen standard, single-header C/C++)
  - Files: [`Modules/Assets/Model/GLTFCookerPlugin.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Assets/Model/GLTFCookerPlugin.cpp), [`Modules/Assets/Model/OBJCookerPlugin.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Assets/Model/OBJCookerPlugin.cpp)
  - Implement `SMikkTSpaceInterface` callbacks (`m_getPosition`, `m_getNormal`, `m_getTexCoord`, `m_setTSpaceBasic`).
  - Calculate 4-component tangent vectors ($T_x, T_y, T_z$ + handedness sign $T_w = \pm 1.0$), ensuring zero normal map seams across mirrored UVs in Blender/Substance/glTF models.
- [ ] **4.1.2 meshoptimizer Cache & Fetch Optimization**
  - Library: `meshoptimizer` (`zeux/meshoptimizer` via `vcpkg`)
  - Prior to clustering, run:
    - `meshopt_optimizeVertexCache()`: reorders indices to maximize hardware post-transform cache hit rate (FIFO / LRU).
    - `meshopt_optimizeOverdraw()`: reorders triangles front-to-back to minimize pixel overdraw.
    - `meshopt_optimizeVertexFetch()`: packs vertex buffers contiguously based on index order to maximize memory bus fetch locality.
- [ ] **4.1.3 meshoptimizer Meshlet Clustering & Bounding Cones**
  - Library: `meshoptimizer`
  - Call `meshopt_buildMeshlets(meshlets, meshlet_vertices, meshlet_triangles, ...)` with:
    - `max_vertices = 64` (Vulkan EXT recommendation).
    - `max_triangles = 124` (4-byte alignment, 124 micro-triangles $\times 3 = 372$ bytes $\le 512$ bytes).
    - `cone_weight = 0.5f` (optimal balance between cluster spatial compactness and normal cone alignment).
  - Call `meshopt_computeMeshletBounds()` to compute bounding sphere (`center`, `radius`) and normal cone (`cone_apex`, `cone_axis`, `cone_cutoff`).
- [ ] **4.1.4 MeshAsset Binary Chunks for Meshlets**
  - Files: [`Engine/Include/Lyra/Assets/Format/MeshAsset.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Include/Lyra/Assets/Format/MeshAsset.h), [`Engine/Source/Assets/Format/MeshAsset.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Assets/Format/MeshAsset.cpp)
  - Extend `.mesh` binary asset format with dedicated chunks:
    - `CHUNK_MSHL` (`0x4C48534D`): Array of `MeshletDescriptor`:
      ```cpp
      struct MeshletDescriptor {
          uint32_t vertex_offset;    // offset into meshlet_vertices array
          uint32_t triangle_offset;  // offset into meshlet_triangles array
          uint32_t vertex_count;     // <= 64
          uint32_t triangle_count;   // <= 124
      };
      ```
    - `CHUNK_MVRX` (`0x5852564D`): Array of `uint32_t` global vertex indices referenced by meshlets.
    - `CHUNK_MTRI` (`0x4952544D`): Packed array of `uint8_t[3]` micro-triangle indices (local vertex index within $[0, 63]$).
    - `CHUNK_MBND` (`0x444E424D`): Array of `MeshletBounds`:
      ```cpp
      struct MeshletBounds {
          Vector3 center;
          float   radius;
          int8_t  cone_axis[3];      // quantized signed 8-bit normal cone direction
          int8_t  cone_cutoff;       // quantized cos(half_angle) in [-1, 1]
          Vector3 cone_apex;
      };
      ```

---

## 4.2 Two-Phase GPU-Driven Occlusion Culling & Execution
- [ ] **4.2.2 GPU Task & Mesh Shader Pipeline**
  - Shaders: [`Engine/Assets/Shaders/GPUDriven/Culling.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/GPUDriven/Culling.slang), `Engine/Assets/Shaders/GPUDriven/MeshShader.slang`
  - **Task / Amplification Shader (`[shader("amplification")]`)**:
    - Workgroup: `[numthreads(32, 1, 1)]` (1 thread per meshlet).
    - Frustum culling: test bounding sphere against 6 camera frustum planes.
    - Backface cone culling: `dot(cone_axis, normalize(cone_apex - camera_pos)) >= cone_cutoff`.
    - Hi-Z Occlusion: project bounding sphere to screen AABB, compute mip level $\text{mip} = \lfloor \log_2(\max(w, h)) \rfloor$, sample 4 depth texels, discard if occluded.
    - Prefix sum via `WavePrefixSum()` / `WaveActiveCountBits()`; emit `DispatchMesh(visible_count, 1, 1, payload)`.
  - **Mesh Shader (`[shader("mesh")]`)**:
    - Workgroup: `[outputtopology("triangle")] [numthreads(32, 1, 1)]`.
    - Call `SetMeshOutputCounts(meshlet.vertex_count, meshlet.triangle_count)`.
    - Transform vertex positions, normals, and UVs; write primitive micro-indices to `SV_PrimitiveIndices`.
- [ ] **4.2.3 MultiDrawIndirect Fallback Path**
  - File: [`Modules/Rendering/GPUDriven/GPUDrivenPlugin.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Rendering/GPUDriven/GPUDrivenPlugin.cpp)
  - Compute shader culling outputting `VkDrawIndexedIndirectCommand` / `D3D12_DRAW_INDEXED_ARGUMENTS` buffers for hardware without mesh shader support.
- [ ] **4.2.4 GPU-Driven Meshlet Visual Test Fixture**
  - Directory: `Tests/gpu_driven_meshlets/`
  - Render dense multi-instance meshlet scene (Stanford Dragon / Lucy, >500k triangles) with heavy camera occlusion.
  - Verify rendered output matches non-culled golden `reference.png` with `RMSE < 1.0` and `mismatch < 0.5%`.
  - Validate with `just test gpu_driven_meshlets`.
