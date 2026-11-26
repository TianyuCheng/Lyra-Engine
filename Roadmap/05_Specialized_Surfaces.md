# Milestone 5: Specialized Surface Renderers (Ocean, Terrain, Skinning)

**Goal**: Complete the specialized surface rendering shaders and deformation systems with automated perceptual image verification.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 5.1 Continuous LOD Terrain Rendering (CDLOD)
- [ ] **5.1.1 Quadtree Chunking & Tri-Planar Material Splatting**
  - Shader: [`Engine/Assets/Shaders/Terrain/Terrain.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/Terrain/Terrain.slang)
  - Quadtree $64 \times 64$ grid chunking with smooth morphing factor:
    $$\alpha = \text{saturate}\left(\frac{2 \cdot \text{distance} - d_{\min}}{d_{\max} - d_{\min}}\right)$$
  - Tri-planar material splatting with slope-based rock/grass/snow blending and normal maps.
- [ ] **5.1.2 Terrain Visual Test Fixture**
  - Directory: `Tests/terrain/`
  - Render synthetic mountain/valley heightmap with multi-layer texturing; commit `reference.png`.
  - Validate with `just test terrain`.

---

## 5.3 Skeletal Mesh Skinning & Animation Deformation
- [ ] **5.3.1 glTF Skinning Importer & GPU Bone Palette**
  - Shader: [`Engine/Assets/Shaders/Common/Skinning.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/Common/Skinning.slang)
  - File: [`Engine/Source/Rendering/RenderWorld.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Rendering/RenderWorld.cpp)
  - Extract bone joint nodes, inverse bind matrices, and vertex `JOINTS_0` / `WEIGHTS_0` into `MeshAsset` binary cache.
  - Upload animated skeleton matrices ($M = \sum_{i=0}^3 w_i B_i$) to structured bone palette buffer.
  - Evaluate linear blend skinning (LBS) or dual quaternion skinning (DQS) in vertex shaders.
- [ ] **5.3.2 Skeletal Animation Visual Test Fixture**
  - Directory: `Tests/skinning/`
  - Render posed/animated skinned character mesh at fixed keyframe; commit `reference.png`.
  - Validate with `just test skinning`.
