# Milestone 7: Physics, Spatial Audio & High-Level Scripting

**Goal**: Complete the gameplay foundations by introducing multi-threaded physics simulation debug passes, streamable 3D spatial sound asset pipelines, and high-level language scripting.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 7.1 Physics & Collision System (Jolt Physics Integration)
- [ ] **7.1.4 Physics Debug Wireframe Pass**
  - Draw live collider wireframes, contact normals, and center-of-mass gizmos in `SceneView` viewport.

---

## 7.2 3D Spatial Audio Subsystem (miniaudio)
- [ ] **7.2.5 Audio Asset Cooker & Streamable Binary Format**
  - Header: `Engine/Include/Lyra/Assets/Format/AudioAsset.h`
  - Cooker: `Modules/Assets/Audio/AudioCookerPlugin.cpp`
  - Cook `.wav`, `.ogg`, `.mp3`, `.flac` into streamable compressed binary `.audio` files using AMS dependencies and deterministic GUID resolution.
  - Large audio file thresholding for disk streaming vs. full in-memory PCM caching.

---

## 7.3 High-Level Scripting (Lua / Luau)
- [ ] **7.3.1 Luau / Sol2 Integration**
  - Module: `Modules/Scripting/Lua/`
  - Integrate Luau or Sol2 (Lua 5.4) sandbox into the engine runtime.
- [ ] **7.3.2 Reflection & ECS API Bindings**
  - Bind `Entity`, `TransformComponent`, `Input`, `Audio`, `Physics`, and logging utilities to Lua scripts.
- [ ] **7.3.3 Live Script Hot-Reloading**
  - Detect changes to `.lua` files on disk via `AMSWatcher` and reload script tables in place without interrupting simulation.

---

## 7.4 Python Ecosystem Integration (`nanobind` & AI / Simulation Subsystem)
- [ ] **7.4.1 `nanobind` C++20 Core Reflection Bindings**
  - Module: `Modules/Scripting/Python/`
  - Integrate `nanobind` for lightweight, ultra-fast compile-time C++20 Python bindings.
  - Expose core Lyra types: `AppContext`, `World`, `SceneNode`, `Entity`, `TransformLocal`, `TransformWorld`, `Camera`, `Light`, `PhysicsBody`, `InputManager`, and logging.
- [ ] **7.4.2 Vectorized Zero-Copy EnTT Buffer Protocol & DLPack Interop**
  - Eliminate per-entity Python loop overhead by binding flat EnTT component arrays to Python buffer protocol:
    - Expose contiguous component data pools directly as zero-copy NumPy array views (e.g. `world.view_array(TransformWorld, "position")` returning `float32[:, 3]` view over C++ memory).
    - Enables high-performance vectorized SIMD math (`positions += velocities * dt`) at hundreds of frames per second.
  - **DLPack Tensor Interop**:
    - Direct GPU-to-GPU zero-copy tensor sharing between Lyra `GPUBuffer` / render targets and **PyTorch Tensors** / **CUDA arrays** via `DLPack` specification.
- [ ] **7.4.3 Python Project Extensions (`ExtensionAPI`)**
  - Provide base Python class `lyra.Extension` mirroring native C++ `ExtensionAPI`:
    ```python
    import lyra
    import numpy as np

    class ProceduralCityExtension(lyra.Extension):
        def init(self, context: lyra.AppContext):
            print("Initialized Python extension with NumPy support")

        def update(self, context: lyra.AppContext, delta_time: float):
            pass

        def shutdown(self, context: lyra.AppContext):
            pass
    ```
- [ ] **7.4.4 Standalone `import lyra` Python Package Mode (Headless AI/RL)**
  - Compile Lyra runtime as a standalone Python C-extension module/wheel (`lyra.so` / `lyra.pyd`).
  - Enable running Lyra headlessly inside PyTorch training loops, robotics reinforcement learning environments (Isaac Gym / MuJoCo style), and Jupyter notebooks.
