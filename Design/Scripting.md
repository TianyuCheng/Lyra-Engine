# Scripting System

The **Scripting System** (`Lyra/Scripting/`) implements high-performance, native C++ ECS systems loaded dynamically through module boundaries. Instead of embedding a heavy interpreted language (like Lua or C#) with language-boundary marshalling and garbage collection overhead, Lyra executes compiled C++ systems against an [EnTT](https://github.com/skypjack/entt)-backed entity-component system.

## Module Interface (`ScriptAPI`)

Script modules export `ScriptAPI`, defining systems, reflected component inspectors, and parameters:

```cpp
struct ScriptAPI
{
    CString (*get_api_name)()                                   = nullptr;
    uint (*get_scripts)(ScriptDescriptor* out)                  = nullptr;
    void (*run)(ScriptID id, ScriptContext& ctx)                = nullptr;
    uint (*get_params)(ScriptID id, ScriptFieldDescriptor* out) = nullptr;
    uint (*get_components)(ComponentDescriptor* out)            = nullptr;
};
```

### System Metadata (`ScriptDescriptor`)
Systems declare execution metadata at registration:
- **`name`**: Unique system name (hashed into a 64-bit `ScriptID`).
- **`group`**: Hierarchical category path (e.g., `"Gameplay/Camera"`).
- **`stage`**: Pipeline stage when the system executes (e.g., `AppEvent::UPDATE`, `AppEvent::FIXED_UPDATE`).
- **`flags`**: Execution flags, such as `ScriptFlag::RUN_IN_EDITOR` to permit execution outside of play mode.
- **`queries`**: Component dependency declarations used by the scheduler to analyze read/write dependencies.

## Build System & Code Generation (`lyra_bindgen`)

Manually writing `ScriptAPI` dispatch tables, system reflection, and Dear ImGui property editors is tedious and error-prone. Lyra automates this through the `lyra-bindgen` tool (`Tools/BindGen/`) and the `lyra_bindgen()` CMake function (`Tools/Configs/Project.cmake`).

### Workflow
1. **Source Attributes**: Developers annotate C++ structs with `[[lyra::component(...)]]` and functions with `[[lyra::system(...)]]`, adding field attributes like `[[lyra::range]]` or `[[lyra::edit(euler)]]`.
2. **Build-Time Code Generation**: CMake triggers `lyra-bindgen` on the specified headers and source files before compiling the target.
3. **Generated Artifacts**: The tool outputs `${MODULE}.gen.cpp`, which automatically generates:
   - System execution wrappers (`sys__run(ctx)`).
   - Component reflection descriptors and ImGui inspector widgets (`draw_inspector`).
   - The `script_descriptors` and `component_descriptors` static tables.
   - The module factory `create_script_api() -> ScriptAPI`.

### CMake Configuration
A typical script module's `CMakeLists.txt` uses `lyra_module()` and `lyra_bindgen()`:

```cmake
# declare module target
lyra_module(camera-control OBJECT)

# trigger code generation for components and systems
lyra_bindgen(lyra-camera-control
    MODULE CameraControl
    PREFIX Lyra/Scene
    HEADERS
        CameraControl.h
    SOURCES
        CameraControl.cpp
)

target_sources(lyra-camera-control PRIVATE
    CameraControl.h
    CameraControl.cpp
)
```

## Execution Facade (`ScriptContext`)

Systems receive a `ScriptContext` facade rather than direct references to `World` or `Registry`. This restricts access to safe operations, prevents unchecked entity mutations during iteration, and provides per-frame utilities:

- **Frame Timing**: `ctx.dt()` and `ctx.time()`.
- **Input State**: Key states, mouse positions/deltas, actions, and axes (`ctx.is_key_down()`, `ctx.mouse_delta()`, `ctx.get_axis()`).
- **Scratch Memory**: `ctx.scratch()` provides a frame-allocated `MemoryArena` for temporary buffers that automatically resets each frame without heap churn.
- **Transform Manipulation**: Built-in helpers to translate, rotate, and scale nodes (`ctx.translate()`, `ctx.rotate()`, `ctx.scale()`).

### Entity Iteration & Queries

Systems query components using structured binding loops or lambda visitors:

#### Structured Binding Queries
```cpp
for (auto [node, transform, velocity] : ctx.query<TransformLocal, Velocity>())
{
    transform.position += velocity.direction * velocity.speed * ctx.dt();
}
```

#### Batched `each()` Iteration
For tighter inner loops, `ctx.each()` invokes a callable directly on matching entities:

```cpp
ctx.each<TransformLocal, Velocity>([&](SceneNode node, TransformLocal& t, Velocity& v) {
    t.position += v.direction * v.speed * ctx.dt();
});
```

### Automatic `Disabled<T>` Filtering
Components can be disabled without destroying or removing them from entities by wrapping them in `Disabled<T>`. When iterating through `ctx.each()` or `ctx.query()`, the context checks for active `Disabled<T>` storage and automatically excludes entities marked with matching disabled components via `entt::exclude`.

### Deferred Entity Commands
Modifying entity hierarchies or adding/removing components during query iteration can invalidate registry iterators. `ScriptContext` routes structural operations through a deferred `ScriptCommandQueue`:

```cpp
// queued commands execute safely after the system finishes
ctx.create([](SceneNode new_node) {
    // initialize new entity
});

ctx.add_component<Rigidbody>(node, mass, friction);
ctx.remove_component<ParticleEmitter>(node);
ctx.destroy(node);
```

The `ScriptLayer` flushes the command queue at designated synchronization points between pipeline stages.

## Component Reflection & Inspector

Script modules can expose custom components to the editor by registering `ComponentDescriptor`:

```cpp
struct ComponentDescriptor
{
    CString name                                         = nullptr;
    CString category                                     = nullptr;
    CString icon                                         = nullptr;
    bool (*has_component)(World& world, SceneNode node)  = nullptr;
    void (*draw_inspector)(World& world, SceneNode node) = nullptr;
};
```

When an entity is selected in the editor, `draw_inspector` is invoked to render Dear ImGui controls for modifying component properties at runtime.

## Practical Example: Implementing a Gameplay System

The following example defines a custom component with reflection attributes and an update system that uses `ScriptContext` to query entities, read input, update transforms, and spawn projectiles through the deferred command queue:

```cpp
#include <Lyra/Scripting/ScriptContext.h>
#include <Lyra/Scene/Transform.h>

using namespace lyra;

// 1. define component with reflection attributes
struct [[lyra::component("Player", category = "Gameplay")]] PlayerController
{
    [[lyra::range(1.0f, 20.0f)]]
    float move_speed = 5.0f;

    [[lyra::range(0.1f, 2.0f)]]
    float fire_cooldown = 0.5f;

    float last_fire_time = 0.0f;
};

struct Projectile
{
    Vector3 direction;
    float   speed = 25.0f;
};

// 2. define the system logic
[[lyra::system(UPDATE, group = "Gameplay/Player")]]
inline void update_players(ScriptContext& ctx)
{
    ctx.each<PlayerController, TransformLocal>([&](SceneNode node, PlayerController& player, TransformLocal& xform) {
        // query input axes from facade
        Vector2 move_axis = ctx.get_axis_2d(InputAxis2D::MOVE);
        Vector3 movement  = Vector3(move_axis.x, 0.0f, move_axis.y) * player.move_speed * ctx.dt();
        ctx.translate(xform, movement);

        // check action trigger and cooldown
        if (ctx.is_key_pressed(KeyButton::KEY_SPACE) && (ctx.time() - player.last_fire_time >= player.fire_cooldown)) {
            player.last_fire_time = ctx.time();

            // defer entity creation to the command queue safely during iteration
            ctx.create([spawn_pos = xform.position](SceneNode projectile) {
                TransformLocal proj_xform{};
                proj_xform.position = spawn_pos + Vector3(0.0f, 0.5f, 1.0f);
                projectile.add<TransformLocal>(proj_xform);
                projectile.add<Projectile>(Vector3(0.0f, 0.0f, 1.0f), 30.0f);
            });
        }
    });
}
```
