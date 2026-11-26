# Milestone 8: Packaging, Standalone Player & Headless CI / CD (Distribution)

**Goal**: Package games into self-contained distributions, build a standalone runtime player, and establish automated cloud verification.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 8.1 Encapsulated Engine Subsystems & Runtime Host (`EngineHost` / `DefaultPlugins`)
- [ ] **8.1.1 `DefaultPlugins` Layer Bundle & Standard Cooker Registration**
  - Files: `Engine/Include/Lyra/Runtime/DefaultPlugins.h`, `Engine/Source/Runtime/DefaultPlugins.cpp`
  - Encapsulate boilerplate layer binding into a single declarative invocation (`app.bind_default_plugins()`):
    - Automatically binds `TimingLayer`, `InputLayer`, `AssetLayer`, `RenderLayer`, `SceneLayer`, and `ScriptLayer` in strict deterministic order.
    - Automatically registers all built-in loaders and cookers:
      - Core assets: `TextAsset`, `JsonAsset`, `TomlAsset`, `MeshAsset`, `MaterialAsset`, `SceneAsset`.
      - Texture cookers: STB, EXR, DDS, KTX (`TextureAsset`).
      - Model cookers: STL, OBJ, glTF (`ModelAsset`).
      - Audio & preview cookers.
- [ ] **8.1.2 Unified `EngineHost` Application Lifecycle & CLI Option Parser**
  - Files: `Engine/Include/Lyra/Runtime/EngineHost.h`, `Engine/Source/Runtime/EngineHost.cpp`
  - Standardized CLI option parser (`--project`, `--gfxapi`, `--renderer`, `--scene`, `--vsync`, `--fullscreen`, `--width`, `--height`).
  - Standardized VFS mount resolution for engine (`sys://`) and project (`app://`) assets and cache hierarchies.
  - High-level bootstrap API:
    ```cpp
    lyra::EngineHost host("Lyra Game Player");
    host.configure_player(argc, argv);
    return host.run();
    ```
- [ ] **8.1.3 Shared `SceneRenderPipeline` Abstraction (Player & Editor Viewport)**
  - Files: `Engine/Include/Lyra/Rendering/SceneRenderPipeline.h`, `Engine/Source/Rendering/SceneRenderPipeline.cpp`
  - Extract common rendering orchestration out of `Samples/Editor/Renderer.cpp`:
    - Hierarchy matrix propagation and camera uniform buffer updates.
    - Depth buffer pooling, reallocation, and format resolution on resize.
    - Unified FrameGraph pass scheduling and command buffer presentation synchronization.
  - Player executes `SceneRenderPipeline` directly to swapchain backbuffer; `EditorRenderer` wraps `SceneRenderPipeline` and attaches editor-only passes (`GridPass`, selection outlines, debug gizmos).
- [ ] **8.1.4 Editor & Player Main Entry Point Refactoring**
  - Files: [`Samples/Editor/Main.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Main.cpp), [`Samples/Editor/Renderer.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Renderer.cpp), `Samples/Player/Main.cpp`
  - Refactor `Samples/Editor/Main.cpp` to use `EngineHost`, removing over 200 lines of manual layer instantiation and asset registration boilerplate.

---

## 8.2 Standalone Game Player Target (`Samples/Player`)
- [ ] **8.2.1 Player Executable Project Setup**
  - Create `Samples/Player` target in CMake without editor UI dependencies.
- [ ] **8.2.2 Runtime-Only Boot Flow**
  - Initialize window, mount VFS `.pak`, load startup scene, and execute application loop with minimal memory footprint.

---

## 8.3 Headless CI / CD & Perceptual Regression Testing
- [ ] **8.3.1 GitHub Actions Headless Workflow**
  - File: `.github/workflows/ci.yml`
  - Automate `just test` execution in GitHub Actions using `SoftGPU` (AVX2 rasterizer + Embree raytracing) and `MockGPU` without physical GPU runners.

---

## 8.4 Static Analysis, Linter Infrastructure & Code Quality (`just lint`)
- [ ] **8.4.1 Clang-Tidy Configuration & Architectural Rules Enforcement**
  - File: [`.clang-tidy`](file:///Users/tcheng/Repositories/Lyra-Engine/.clang-tidy)
  - Configure modernize, bugprone, performance, and readability checks.
  - Enforce Lyra architectural conventions:
    - View/span semantics over raw pointers (`Span<T>`, `StringView`, `GPUBufferSlice`).
    - Flag raw STL type usages (`std::vector`, `std::string`, `std::mutex`) where Lyra aliases are required (`Vector<T>`, `String`, `Mutex`).
    - Avoid expensive accidental value copies in container loops (`performance-for-range-copy`).
    - Enforce explicit constructors for single-parameter non-converting types (`google-explicit-constructor`).
- [ ] **8.4.2 `just lint` Workflow Recipe & Parallel Runner**
  - Files: [`Justfile`](file:///Users/tcheng/Repositories/Lyra-Engine/Justfile), `Tools/Scripts/run_lint.py`
  - Implement `just lint [fix]` command utilizing multi-threaded `run-clang-tidy` across CMake compilation database (`compile_commands.json`).
  - Provide `--fix` flag to automatically apply non-destructive modernize suggestions.
- [ ] **8.4.3 Automated CI Quality Gate**
  - File: `.github/workflows/ci.yml`
  - Add pull request check executing `just format-check` and `just lint`, failing the build if unformatted files or high-severity linter warnings are detected.

---

## 8.5 Programmatic Umbrella Header Generation & Validation (`just headers`)
- [ ] **8.5.1 Two-Tier Subsystem Header Architecture**
  - Subsystem umbrella headers (`Engine/Include/Lyra/`):
    - `Common.hpp`: Core utilities, scalar aliases, math, string, pointers, collections, ECS, sync, logging.
    - `Graphics.hpp`: RHI contracts, descriptors, buffers, textures, command encoders, retire queue.
    - `Rendering.hpp`: FrameGraph, RenderWorld, RenderScene, Geometry, system textures, built-in passes.
    - `Scene.hpp`: World, SceneNode, SceneTree, and all core components (`Transform`, `Mesh`, `Camera`, `Light`, `Sun`).
    - `Assets.hpp`: AMS server, registry, preview, and asset codecs (`Text`, `Json`, `Toml`, `Mesh`, `Model`, `Texture`, `Material`).
    - `Physics.hpp`: Physics contracts, collision shapes, queries, layers, events.
    - `Input.hpp`: InputManager, maps, bindings, filters.
    - `UI.hpp`: GUI contracts, immediate-mode widget wrappers (`Tree`, `Dock`, `Dialog`, `Command`).
    - `Runtime.hpp`: Application, AppContext, EngineHost, default layer stack.
  - Root umbrella header ([`Engine/Include/Lyra/Lyra.hpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Include/Lyra/Lyra.hpp)):
    - Reduced from a manual 220-line list to an aggregated master header including only the 9 tier-1 subsystem headers.
- [ ] **8.5.2 Automated Generator Script (`Tools/Scripts/generate_headers.py`)**
  - File: `Tools/Scripts/generate_headers.py`
  - Scan `Engine/Include/Lyra/` directories recursively to discover public headers.
  - Exclude private/implementation details: `detail/*`, `internal/*`, `*.inl`, and test fixtures.
  - Sort deterministically in alphabetical order and format with standard Lyra header comment banners and `#pragma once` guards.
  - Provide `--check` mode returning non-zero exit code if generated files differ from disk (for CI verification).
- [ ] **8.5.3 Header Self-Containment Validation & `Justfile` Integration**
  - File: [`Justfile`](file:///Users/tcheng/Repositories/Lyra-Engine/Justfile)
  - Add `just headers` (generate) and `just headers-check` (verify) command recipes.
  - Verification: Generate transient single-include translation units for each umbrella header and compile them independently in CI to verify full self-containment without hidden transitive dependency assumptions.

---

## 8.6 Dynamic Project Extension Architecture (CMake + C++ / Lua / Python)
- [ ] **8.6.1 Multi-Extension Architecture & `ExtensionAPI` (`Plugin<ExtensionAPI>`)**
  - Standardized C-ABI interface table reusing Lyra's existing plugin architecture ([`Lyra/Utilities/Plugin.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Include/Lyra/Utilities/Plugin.h)):
    ```cpp
    struct ExtensionAPI
    {
        const char* (*get_name)();
        void (*init)(AppContext& context);
        void (*update)(AppContext& context, float delta_time);
        void (*shutdown)(AppContext& context);
    };

    using ExtensionPlugin = Plugin<ExtensionAPI>;
    ```
  - **`ExtensionManager` Subsystem**:
    - Files: `Engine/Include/Lyra/Runtime/ExtensionManager.h`, `Engine/Source/Runtime/ExtensionManager.cpp`
    - Manages an ordered collection of active project extensions (`Vector<Own<ExtensionPlugin>> extensions`).
    - Discovers and loads multiple extension dynamic libraries specified in `Project.toml`:
      ```toml
      [project]
      name = "InteractiveSimulation"
      version = "1.0.0"
      startup_scene = "app://Scenes/Main.scene"

      [extensions]
      # Loaded sequentially in deterministic order; unloaded in reverse order
      plugins = [
          "Binaries/SimCore",
          "Binaries/VehicleDynamics",
          "Binaries/SensorSimulation",
      ]
      ```
    - Sequential initialization on boot: calls `api->init(context)` in list order.
    - Per-frame dispatch: calls `api->update(context, delta_time)` during application tick.
    - Safe teardown: calls `api->shutdown(context)` in reverse list order and unloads libraries.
- [ ] **8.6.2 Standalone Extension Project Template**
  - Project directory layout:
    ```
    MyProject/
    ├── Project.toml              # Project manifest with [extensions] list
    ├── CMakeLists.txt            # Minimal CMake script building dynamic extension libraries
    ├── Source/
    │   ├── Extension.cpp         # Implements ExtensionAPI and exports create_plugin()
    │   └── Components/           # Domain-specific ECS components and systems
    ├── Assets/                   # Raw assets (scenes, textures, models, scripts)
    └── Binaries/                 # Output directory for compiled extension binaries
    ```
  - Implementation in `Source/Extension.cpp`:
    ```cpp
    #include <Lyra/Lyra.hpp>
    #include <Lyra/Runtime/ExtensionAPI.h>

    using namespace lyra;

    static ExtensionAPI create_extension_api()
    {
        ExtensionAPI api{};
        api.get_name = []() -> const char* { return "VehicleDynamics"; };
        api.init = [](AppContext& context) {
            // Register ECS components and add systems to context.services
        };
        api.update = [](AppContext& context, float dt) {
            // Per-frame extension simulation tick
        };
        api.shutdown = [](AppContext& context) {
            // Cleanup extension resources before unload
        };
        return api;
    }

    LYRA_EXPORT ExtensionAPI create_plugin()
    {
        return create_extension_api();
    }
    ```
- [ ] **8.6.3 Unified Multi-Language Scripting Integration**
  - **Native C++**: Performance-critical gameplay loops, vehicle/robotics controllers, custom rendering passes, and heavy math.
  - **Lua / Luau**: Fast-iteration behavior scripts and UI event handling bound to EnTT entities via `sol2`.
  - **Python**: Editor automation, procedural level generation, and pipeline tasks bound via `pybind11` or invoked as CLI tool scripts.
- [ ] **8.6.4 Shipped Application Distribution Flow (`just package-app`)**
  - Automation recipe cooking all project assets into compressed `.pak` archives (`Data/Game.pak`).
  - Assembles self-contained standalone distribution bundle:
    ```
    MyProject_Release/
    ├── App.exe                   # Standalone player executable (renamed lyra-player)
    ├── lyra-engine.dll           # Core engine runtime
    ├── Binaries/                 # All shipped project extension DLLs
    │   ├── SimCore.dll
    │   └── VehicleDynamics.dll
    └── Data/Game.pak             # Packed assets
    ```

---

## 8.7 Modular Library & SDK Distribution Model (`find_package(Lyra)`)
- [ ] **8.7.1 Modular CMake Targets Export**
  - Files: `CMakeLists.txt`, [`Tools/Packages/LyraEngineConfig.cmake.in`](file:///Users/tcheng/Repositories/Lyra-Engine/Tools/Packages/LyraEngineConfig.cmake.in)
  - Split monolithic target into granular modular interface/imported targets:
    - `Lyra::Common` (Utilities, math, collections, ECS, sync)
    - `Lyra::Graphics` (RHI contracts, descriptors, backends, shader pipeline)
    - `Lyra::Rendering` (FrameGraph, geometry, system textures, passes)
    - `Lyra::Physics` (Jolt physics abstraction, collision, queries)
    - `Lyra::Runtime` (Application host, default layers)
  - Support consumer CMake configuration:
    ```cmake
    find_package(Lyra REQUIRED COMPONENTS Graphics Rendering Physics)
    target_link_libraries(MyCADApp PRIVATE Lyra::Graphics Lyra::Physics)
    ```
- [ ] **8.7.2 CMake `FetchContent` Source Embedding**
  - Add configuration switches allowing external projects to embed Lyra directly via `FetchContent_Declare`:
    - `LYRA_BUILD_EDITOR=OFF`
    - `LYRA_BUILD_SAMPLES=OFF`
    - `LYRA_BUILD_TESTS=OFF`
- [ ] **8.7.3 Precompiled Binary SDK Packaging via CPack (`just package-sdk`)**
  - Implement CPack configuration generating self-contained SDK archive bundles (`Lyra-SDK-<version>-<os>-<arch>.tar.gz` / `.zip`) containing headers (`Engine/Include/Lyra/`), prebuilt shared/static libraries, and CMake export targets.

---

## 8.8 Python Runtime Packaging & Environment Interop
- [ ] **8.8.1 Embedded Slim Python Distribution**
  - Package an isolated, portable Python standard library archive (e.g. Windows embeddable zip / macOS slim framework) alongside the standalone player and editor distributions.
  - Ensures end users without Python pre-installed can run Python-based extensions and automation scripts out of the box.
- [ ] **8.8.2 External Virtualenv & Conda Auto-Attachment**
  - Detect active terminal virtual environment variables (`VIRTUAL_ENV`, `CONDA_PREFIX`) on engine boot.
  - Dynamically prepend virtualenv `site-packages` to Python's `sys.path`, allowing immediate in-engine access to external scientific and deep learning packages (`torch`, `numpy`, `scipy`, `cv2`, `open3d`) installed via `pip`.
