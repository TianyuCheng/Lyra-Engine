# Scene Architecture

The **Scene Subsystem** (`Lyra/Scene/`) implements a hybrid architecture combining an entity-component system (ECS) with an auxiliary hierarchical scene graph. 

While pure ECS flat-pools maximize cache locality for parallel data processing, 3D rendering and gameplay require hierarchical scene graphs for parent-child spatial attachment, relative motion, and recursive transform evaluation. Lyra bridges both worlds by pairing [EnTT](https://github.com/skypjack/entt) with an incremental `SceneTree`.

## Core Components of the Hierarchy

### `World` and `SceneNode`
The `World` struct owns the central ECS `Registry` (`entt::registry`). Entities are represented through `SceneNode`, a lightweight wrapper that couples an `Entity` identifier with convenience methods for hierarchy navigation and component lookup:

```cpp
World world;
SceneNode node = world.create_node("MainCamera");
node.add<Camera>();
```

### `SceneTree`
`SceneTree` is an auxiliary data structure that maintains parent-child relationships using an index-based tree:

```cpp
struct Node
{
    Entity    entity;
    NodeIndex parent       = INVALID_NODE;
    NodeIndex first_child  = INVALID_NODE;
    NodeIndex next_sibling = INVALID_NODE;
    uint      depth        = 0;
};
```

Instead of requiring manual tree management, `SceneTree` listens to EnTT component signals to keep the hierarchy synchronized with entity creations, destructions, and parenting changes.

### Transform Propagation
Transform evaluations are split into two components:

- **`TransformLocal`**: Defines local position (`Vector3`), scale (`Vector3`), and rotation (`Quaternion`). Includes dirty flags (`TransformFlag::LOCAL_DIRTY`, `TransformFlag::WORLD_DIRTY`).
- **`TransformWorld`**: Contains the evaluated $4 \times 4$ transformation matrix (`Matrix4x4 xform`).

During the scene update phase, `SceneTree::update()` traverses nodes in topological depth order, propagating parent matrices downward to compute final world matrices:

$$\mathbf{M}_{\text{world}} = \mathbf{M}_{\text{parent}} \times \mathbf{M}_{\text{local}}$$

## Built-In Scene Components

### `Camera`
Encapsulates view and projection geometry:
- Perspective and orthographic projection modes.
- Field of view, near/far clipping planes, aspect ratios.
- Generates view and projection matrices adapted to Lyra's reversed-Z $[0, 1]$ depth convention.

### `Light`
Defines illumination sources across three categories:
- **Directional**: Global sun/sky light with orientation and illuminance.
- **Point**: Omnidirectional light with position, range, and quadratic attenuation.
- **Spot**: Cone-restricted light with inner and outer cutoff angles.

### `Mesh`
Binds rendering assets to scene nodes:
- References `AssetHandle<MeshAsset>`.
- Stores submesh indices and material asset references.

## Scene Management (`SceneManager`)

The `SceneManager` handles loading, instantiating, and transitioning scenes:

### Loading Modes
- **`LoadMode::SINGLE`**: Clears all non-persistent entities in the current `World` and activates the newly loaded scene.
- **`LoadMode::ADDITIVE`**: Spawns entities from the scene file into the existing world without clearing current entities.

### Instantiating Prefabs and Models
Models imported from formats like glTF contain internal hierarchies. `SceneManager::spawn_model()` unpacks `ModelAsset` hierarchies into corresponding `SceneNode` entities, recreating node transforms, meshes, and material assignments under a specified parent node.

## Practical Example: Constructing a Scene Hierarchy

The following example shows creating a parent entity, attaching child nodes with camera and light components, transforming local coordinates, and evaluating the world hierarchy:

```cpp
#include <Lyra/Scene/World.h>
#include <Lyra/Scene/Camera.h>
#include <Lyra/Scene/Light.h>
#include <Lyra/Scene/SceneTree.h>

using namespace lyra;

void construct_scene()
{
    World world;
    SceneTree tree(world);

    // 1. create parent scene node
    Entity root = world.create();
    world.translate(root, Vector3(0.0f, 1.0f, 0.0f));

    // 2. create child camera node
    Entity camera_entity = world.create();
    world.add_child(root, camera_entity);
    world.translate(camera_entity, Vector3(0.0f, 2.0f, -5.0f));

    auto& cam = world.registry.emplace<Camera>(camera_entity);
    cam.fov  = 60.0f;
    cam.near = 0.1f;
    cam.far  = 1000.0f;

    // 3. create child point light node
    Entity light_entity = world.create();
    world.add_child(root, light_entity);
    world.translate(light_entity, Vector3(2.0f, 3.0f, 0.0f));

    auto& light = world.registry.emplace<Light>(light_entity);
    light.type      = LightType::POINT;
    light.color     = Vector3(1.0f, 0.9f, 0.7f);
    light.intensity = 500.0f;

    // 4. evaluate topological order and propagate transforms
    tree.update();

    // query evaluated world matrix of the camera
    const auto& cam_world = world.registry.get<TransformWorld>(camera_entity);
    Vector3 final_cam_pos = Vector3(cam_world.xform[3]);
}
```
