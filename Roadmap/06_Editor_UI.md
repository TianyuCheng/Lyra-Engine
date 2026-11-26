# Milestone 6: Editor Features, UI System & Iteration Workflow

**Goal**: Build a responsive, fully featured editor environment in `Samples/Editor` and `Engine/UISystem` with scene manipulation, live asset reloading, visual inspectors, and robust undo/redo capabilities.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 6.1 Viewport Controls & Canvas Tools
- [ ] **6.1.3 Viewport Shading Modes & Camera Controls**
  - Files: [`Samples/Editor/Panels/SceneView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/SceneView.cpp), [`Modules/Scripting/CameraControl/CameraControl.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Scripting/CameraControl/CameraControl.cpp)
  - Add viewport shading selector dropdown: Lit (PBR), Unlit, Wireframe, Normal Buffer, Depth Buffer, Roughness/Metallic, Overdraw visualization.
  - Complete navigation modes: smooth orbit (Alt + Left drag), fly-cam (WASD + Right drag with speed scroll), and focus framing (F key on selected entity).
  - Add Viewport Orientation Gizmo (view cube / axis tripod) for snapping to Top, Front, Right orthographic views.

---

## 6.2 Scene Hierarchy & Outliner (`HierarchyView`)
- [ ] **6.2.1 Drag-and-Drop Entity Reparenting**
  - File: [`Samples/Editor/Panels/HierarchyView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/HierarchyView.cpp)
  - Support dragging entities onto other entities to re-parent within the `SceneTree` hierarchy; drag to empty space to unparent to scene root.
- [ ] **6.2.2 Multi-Selection, Visibility & Lock Toggles**
  - File: [`Samples/Editor/Panels/HierarchyView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/HierarchyView.cpp)
  - Add inline visibility eye icon (toggle rendering in viewport) and lock icon (prevent accidental selection/movement).
  - Support multi-selection via Shift+Click (range) and Ctrl+Click (toggle).

---

## 6.3 Inspector & Property Grid (`InspectorView`)
- [ ] **6.3.1 Searchable "Add Component" Dropdown**
  - File: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp)
  - Add searchable popup menu displaying all registered ECS components (`Transform`, `Mesh`, `Light`, `Sun`, `Camera`, `VolumetricFog`, `Cloudscape`, `LocalFogVolume`, `Script`, `Audio`, `Physics`), disabling components already attached to the entity.
- [ ] **6.3.2 Component Actions & Reset Controls**
  - File: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp)
  - Add component header action buttons: collapse/expand, reset to defaults, move up/down, and remove component.
- [ ] **6.3.3 Live PBR Material Editor & Sphere Preview**
  - Files: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp), [`Modules/Assets/Preview/PreviewPlugin.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Assets/Preview/PreviewPlugin.cpp)
  - Visual PBR material inspector with live parameter tuning: Base Color tint & map, Metallic, Roughness, Normal map & scale, Emissive factor, Blend mode, Alpha cutoff.
  - Interactive 3D material preview sphere with environment reflection.
- [ ] **6.3.4 Texture Asset Inspector**
  - File: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp)
  - Visual texture viewer displaying resolution, format, mipmap count, and channel isolation toggles (R, G, B, A, RGB).
- [ ] **6.3.5 Live Renderer Configuration Inspector & Preset Manager**
  - Files: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp), [`Engine/Source/Rendering/RendererManager.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Rendering/RendererManager.cpp)
  - Dedicated Inspector panel view for the active renderer plugin:
    - Automatically generate two-way data-bound UI controls based on the active renderer's `IRendererConfig` table: numerical sliders (bounces, shadow split lambda, culling radii, filter sigmas), boolean checkboxes (denoising, ReSTIR GI/DI, Hi-Z occlusion), and dropdown menus (shading models, debug visualization channels).
    - Quality preset switcher dropdown (`Low`, `Medium`, `High`, `Ultra`, `Cinematic`) with "Save to File", "Load Preset", and "Reset to Defaults" buttons.
    - Synchronize live parameter adjustments to the active rendering loop in real time.

---

## 6.4 Editor Shell, Activity Bar & Collapsible Sidebar Architecture
- [ ] **6.4.1 VS Code-Style Vertical Activity Bar Widget (`ui::activity_bar`)**
  - Files: `Engine/Include/Lyra/UISystem/Widgets/UIActivityBar.h`, `Engine/Source/UISystem/Widgets/UIActivityBar.cpp`, `Samples/Editor/Common/EditorLayout.cpp`
  - Slim vertical toolbar column (~48px width) docked on the far left edge of the editor window:
    - Primary activity icon buttons:
      - `LYRA_ICON_TREE` (Scene Hierarchy & Outliner)
      - `LYRA_ICON_FOLDER` (Asset Browser & VFS Explorer)
      - `LYRA_ICON_SUN` / `LYRA_ICON_LIGHT` (Environment, Atmosphere & Lighting)
      - `LYRA_ICON_GPU` / `LYRA_ICON_SETTINGS` (Renderer TOML Configuration & Quality Profiles)
      - `LYRA_ICON_PROFILER` (CPU/GPU Frame Profiler & Memory Allocations)
      - `LYRA_ICON_CONSOLE` (Diagnostic Console Logs)
    - Bottom pinned utility icons: `LYRA_ICON_SETTINGS` (Editor Preferences & Hotkeys).
    - Visual indicators: Active left accent strip highlight, hover tooltips, and notification badges (e.g. error counters on the console icon, pending cooker tasks).
- [ ] **6.4.2 Collapsible Primary Sidebar Docking Container**
  - Files: [`Samples/Editor/Common/EditorLayout.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Common/EditorLayout.cpp), [`Samples/Editor/Common/EditorLayout.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Common/EditorLayout.h), [`Samples/Editor/Main.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Main.cpp)
  - Left primary sidebar docking container managed by the Activity Bar:
    - Clicking any activity icon switches the active view inside the primary sidebar without rearranging other docked windows.
    - Clicking the currently active icon toggles/collapses the sidebar to 0 width, immediately granting full horizontal screen space to the central 3D viewport.
    - Global shortcut chord (`Ctrl+B` / `Cmd+B`) to toggle sidebar collapse/expand state.
    - Preserves independent persistent right docking for the Inspector and bottom status bar for playback and telemetry.

---

## 6.5 Undo / Redo Architecture & Command History
- [ ] **6.5.1 Centralized Undo/Redo Command Stack**
  - Files: [`Samples/Editor/Common/EditorCommands.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Common/EditorCommands.cpp), [`Samples/Editor/Common/EditorCommands.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Common/EditorCommands.h)
  - Implement Command pattern stack supporting transactional Undo (Ctrl+Z) and Redo (Ctrl+Y / Ctrl+Shift+Z).
  - Wrap common operations: Transform edit, Property tweak, Entity spawn/destroy, Component add/remove, Hierarchy reparenting.
- [ ] **6.5.2 Undo History Panel**
  - File: [`Samples/Editor/Panels/InspectorView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/InspectorView.cpp)
  - Expose an "Undo History" panel showing chronological list of user actions with click-to-revert.

---

## 6.6 Diagnostics & Profiler Panels
- [ ] **6.6.2 GPU & CPU Frame Profiler Panel**
  - Files: [`Samples/Editor/Renderer.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Renderer.cpp), [`Engine/Source/Rendering/FrameGraph.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Rendering/FrameGraph.cpp)
  - Visual timeline bar showing FrameGraph pass timings (Clear, Depth, GBuffer, ShadowMap, Lighting, Skybox, PostProcess) in microseconds.
  - VRAM memory allocation graph: buffers, textures, render targets, acceleration structures.
- [ ] **6.6.3 Viewport Performance HUD Overlay**
  - File: [`Samples/Editor/Panels/SceneView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/SceneView.cpp)
  - Lightweight overlay in viewport corner: FPS, frame time, draw call count, triangle count, meshlet count.
- [ ] **6.6.4 Interactive In-Editor Python REPL Console Panel**
  - File: `Samples/Editor/Panels/PythonConsoleView.cpp`
  - Integrated dockable Python interactive console with command history, multi-line buffer execution, syntax highlighting, and tab completion.
  - Exposes pre-bound engine globals: `app`, `world`, `services`, `selection`.
  - Enables rapid in-editor scene scripting: procedurally spawning entity hierarchies, batch-modifying component parameters, running dataset imports, or triggering offline bake passes directly from the editor UI.

---

## 6.7 Live Asset Hot-Reloading & Scene Persistence
- [ ] **6.7.1 Slang Shader Live Recompilation**
  - Files: [`Engine/Source/Assets/AMSWatcher.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Assets/AMSWatcher.cpp), [`Modules/Compiler/Slang/SlangCompiler.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Compiler/Slang/SlangCompiler.cpp)
  - Recompile modified `.slang` files on file change events, updating active `GPURenderPipeline` instances without restarting.
- [ ] **6.7.2 Texture & Material Live Reloading**
  - File: [`Engine/Source/Rendering/GeometryCache.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Source/Rendering/GeometryCache.cpp)
  - Reload modified image files and rebind texture descriptors on the fly.
- [ ] **6.7.3 Scene State Persistence & Autosave**
  - Files: [`Samples/Editor/EditorState.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/EditorState.cpp), `Engine/Source/Scene/SceneSerializer.cpp`
  - Serialize full EnTT world state, active cameras, lighting rigs, and transform hierarchies to `.scene` JSON/binary files.
  - Implement periodic autosave timer and crash recovery snapshot buffer in user cache directory.
- [ ] **6.7.4 In-Editor Live Shader Diagnostic Banner & Interactive Jump-to-Source**
  - Files: [`Samples/Editor/Panels/SceneView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/SceneView.cpp), [`Samples/Editor/Panels/ConsoleView.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/ConsoleView.cpp), [`Modules/Compiler/Slang/SlangCompiler.cpp`](file:///Users/tcheng/Repositories/Lyra-Engine/Modules/Compiler/Slang/SlangCompiler.cpp)
  - **Diagnostic Parser**:
    - Intercept Slang compiler diagnostic output string (`<file>(<line>): error <code...>: <message>`).
    - Parse diagnostic into structured records: `Path file_path`, `uint line`, `uint column`, `LogLevel severity`, `String error_message`.
  - **Viewport Notification Overlay Banner**:
    - Display floating collapsible translucent red warning banner at top of viewport when any active pipeline shader fails compilation during live reload.
    - Provide single-click hyperlink on each diagnostic line opening the offending `.slang` file directly at the exact line and column in the user's configured editor/IDE (`code -g file:line:col`).
  - **Safe Error Fallback Shader Substitution**:
    - Prevent viewport crash or driver hang when a shader compilation fails; retain existing working pipeline state or automatically bind high-visibility magenta/black checkerboard fallback pipeline until syntax errors are resolved.
- [ ] **6.7.5 Dynamic C++ Project Extension Hot-Reloading & Shadow Copying**
  - Files: `Samples/Editor/EditorExtension.cpp`, `Engine/Include/Lyra/Runtime/ExtensionAPI.h`, [`Engine/Include/Lyra/Utilities/Plugin.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Include/Lyra/Utilities/Plugin.h)
  - **Reusing `Plugin<ExtensionAPI>`**:
    - Manage project dynamic libraries through Lyra's existing plugin infrastructure: `ExtensionPlugin = Plugin<ExtensionAPI>`.
  - **Shadow Copy Mechanism (Windows OS File Lock Prevention)**:
    - Never load the compiler's output file (`Binaries/MyExtension.dll`) directly to prevent `LNK1104` locked file errors during rebuilds.
    - Copy target binary to an incremented revision cache (`Binaries/.hotreload/MyExtension_rev{N}.dll`) before invoking `plugin.load()`.
  - **Cooperative Compilation Triggers & Targeted Multi-Extension Reloading**:
    - **IDE-Driven Build Detection**: `AMSWatcher` monitors `Binaries/`. When an external build finishes writing any extension `.dylib` / `.dll`, the editor isolates the modified extension, unloads it, copies the revision binary, reloads it, and notifies the user via status bar toast.
    - **In-Editor Compile Button**: Status bar "Compile C++" button and `Ctrl+Alt+F11` (`Cmd+B`) hotkey spawning `cmake --build` in background thread, streaming stdout/stderr to [`ConsoleView`](file:///Users/tcheng/Repositories/Lyra-Engine/Samples/Editor/Panels/ConsoleView.cpp) with clickable error hyperlinks.
  - **ECS State Serialization & Recovery**:
    - Temporarily snapshot active component state before calling `api->shutdown(context)`.
    - Reload extension plugin, invoke `api->init(context)`, and restore EnTT component data, preserving scene state across recompiles.

---

## 6.8 Visual Material Graph Editor (`.matgraph`)
- [ ] **6.8.1 Node Graph Data Model & Pin Type System**
  - Files: `Engine/Include/Lyra/Assets/MaterialGraph.h`, `Engine/Source/Assets/MaterialGraph.cpp`
  - **Pin Type System**:
    - Strongly typed data flow supporting `Float`, `Float2`, `Float3`, `Float4`, `Matrix4x4`, `Texture2D`, `Sampler`, and `Boolean`.
    - Implicit type coercion and broadcasting rules (e.g. `Float` automatically broadcasts to `Float3`, vector swizzle masking `.rgb`, `.xy`).
  - **Graph Representation & Serialization**:
    - Directed Acyclic Graph (DAG) structure: `struct MaterialNode`, `struct MaterialPin`, `struct MaterialLink`.
    - Native `.matgraph` JSON asset specification registered with AMS:
      ```json
      {
        "guid": "c3b91a7e-4001-4192-8001-000000000001",
        "type": "material_graph",
        "master_node": "node_pbr_output",
        "nodes": [...],
        "links": [...],
        "exposed_parameters": [...]
      }
      ```
- [ ] **6.8.2 Slang Shader Code Generator (`MaterialGraphCompiler`)**
  - Files: `Modules/Compiler/Slang/MaterialGraphCompiler.h`, `Modules/Compiler/Slang/MaterialGraphCompiler.cpp`
  - **Topological DAG Traversal**:
    - Depth-first search (DFS) topological sort starting from the Master PBR Output node.
    - Automatic dead-code elimination pruning disconnected or unreferenced node subgraphs.
    - Constant folding: pre-evaluate pure arithmetic nodes connected only to static constants during compile time.
  - **Slang Expression Emission**:
    - Generate cleanly structured, portable Slang source code plugging into Lyra's PBR BSDF pipeline:
      ```slang
      import Lyra.PBR.Surface;

      void evaluate_material(in MaterialVertexInput input, out PBRSurfaceParameters surface)
      {
          float2 uv0 = input.uv * _Parameter_Tiling.xy + _Parameter_Offset.xy;
          float4 tex_albedo = textures[_Param_AlbedoTex].Sample(samplers[_Sampler_Linear], uv0);
          surface.base_color = tex_albedo.rgb * _Parameter_ColorTint.rgb;
          surface.roughness = saturate(tex_albedo.a * _Parameter_RoughnessScale);
          surface.metallic = _Parameter_Metallic;
          ...
      }
      ```
    - Expose parameters into uniform buffers (`ConstantBuffer<MaterialGraphParameters>`) with auto-assigned binding slots.
- [ ] **6.8.3 Interactive Node Graph Canvas Panel (`MaterialGraphView`)**
  - Files: `Samples/Editor/Panels/MaterialGraphView.h`, `Samples/Editor/Panels/MaterialGraphView.cpp`
  - Canvas integration using `imgui-node-editor` within `Modules/UISystem/ImGui/`.
  - Infinite pannable and zoomable grid canvas with smooth bezier connection wires and port color coding based on data type.
  - Multi-node selection, box selection, copy/paste/duplicate, and deletion with automatic wire cleanup.
  - **Quick-Search Node Palette**:
    - Contextual right-click / `Space` popup menu with fuzzy keyword search for rapid node insertion.
    - Category-organized node library:
      - **Math**: Add, Subtract, Multiply, Divide, FMA, Power, Sqrt, Min, Max, Clamp, Saturate, Smoothstep, Lerp, Remap.
      - **Vector**: Dot, Cross, Normalize, Length, Distance, Reflect, Refract, Swizzle / Mask, Component Split / Combine.
      - **Geometry & Camera**: World/Object/View Position, Geometric & Tangent Normal, Tangent/Bitangent, Camera Vector, Screen UV, Depth, View Distance.
      - **Texturing**: 2D Texture Sample, Normal Map Unpack, Tri-planar Mapping, Parallax Occlusion, Panner, Rotator, Sub-UV / Flipbook.
      - **Noise & Procedural**: Gradient 2D/3D Noise, Cellular / Worley Noise, Fresnel (Schlick), Wave (Sine, Cosine).
      - **Master Output Node**: Surface PBR target (`BaseColor`, `Metallic`, `Roughness`, `NormalTS`, `AmbientOcclusion`, `Emissive`, `Opacity / ClipMask`, `Displacement`).
- [ ] **6.8.4 Real-Time 3D Shader Ball Viewport & Live Parameter Tweaking**
  - Files: `Samples/Editor/Panels/MaterialPreviewViewport.h`, `Samples/Editor/Panels/MaterialPreviewViewport.cpp`
  - Integrated 3D viewport rendering a classic material shader ball / sphere with orbital camera controls and HDR skybox IBL lighting.
  - **Instant Live Parameter Tuning**:
    - Modifying exposed node parameters in the inspector updates the material instance uniform buffer directly on the GPU without triggering Slang shader recompilation.
  - **Per-Node Intermediate Thumbnail Previews**:
    - Render miniature real-time preview thumbnails directly onto texture and noise nodes, visualizing intermediate values at each stage of the graph.
