# Asset Management System (AMS)

The **Asset Management System (AMS)** manages the lifecycle of engine assets, including discovery, preprocessing (cooking), asynchronous loading, serialization, offline preview rendering, and reference-counted memory management.

## Core Architecture

The system is built around several key components:

1. **AssetServer**: The central orchestrator that manages loading requests, maintains the active asset cache, coordinates worker tasks through the `JobSystem`, and triggers hot-reloading.
2. **AssetRegistry**: A persistent database (binary `.bin` or TOML `.toml`) that maps 64-bit GUIDs (`AssetID`) to source paths and cooked cache paths (`cooked_path`), while maintaining the asset dependency graph. It can also be rebuilt by scanning `.import` files on disk.
3. **Processors**: Modular interfaces for asset handling:
   - **`AssetLoaderAPI`**: Loads cooked binary files into runtime C++ memory structures.
   - **`AssetCookerAPI`**: Preprocesses raw source assets into optimized binary blobs and generates `.import` metadata JSON files.
   - **`AssetSaverAPI`**: Serializes in-memory assets back to disk in the OS filesystem.
   - **`AssetPreviewAPI`**: Renders offline preview scenes and thumbnails into the asset cache.
4. **AMSWatcher**: A background filesystem watcher that monitors the source asset directory for modifications, renames, and deletions.

## Asset Lifecycle

### 1. Importing & Cooking
Before a raw file (e.g., `.png`, `.gltf`, `.obj`) can be used by the engine, it must be cooked into an engine-ready binary format:
- Cooker plugins implement `AssetCookerAPI::process` to convert source files into cached binary blobs.
- Cooking generates a companion `.import` JSON file storing the asset's GUID, type UUID, and dependency records.
- Asynchronous imports are scheduled via `AssetServer::import_asset()`.

### 2. Handles and Reference Counting
Assets are referenced through type-safe `AssetHandle<T>`, which wraps a 64-bit `AssetID` (GUID):
- Loading an asset returns an `AssetHandle<T>` and increments its reference count.
- Unloading decrements the counter.
- When the reference count reaches zero, the asset is marked as a candidate for purging.
- Calling `AssetServer::purge()` frees unused assets from memory (typically during scene transitions).

### 3. Asynchronous Loading
Asset loading is asynchronous by default:
- Calling `load_asset()` checks whether the asset is already resident in memory.
- If not, the server allocates a tracking record and queues a background loading job via `JobSystem`.
- `get_asset()` returns `nullptr` until the background task completes and populates the data.

### 4. Saving & Serialization
Assets created or modified at runtime can be written back to disk via `AssetServer::save_asset()`, which delegates to the registered `AssetSaverAPI` for that asset type.

### 5. File Operations & Hot-Reloading
The `AssetServer` provides APIs to manage assets while keeping metadata in sync:
- `move_asset()` renames or relocates source files along with their `.import` metadata, updating registry paths.
- `delete_asset()` removes source files, `.import` metadata, and cached data, unregistering the asset from the database.
- When directory watching is enabled, file modifications trigger automatic re-cooking and runtime updates via `reload_asset()`.

## Asset Dependencies & Secondary Assets

The AMS supports a declarative dependency graph for external references and secondary assets extracted from container files.

### Container Files and Secondary Assets
Container formats like glTF, OBJ, or scene packages often contain multiple logical resources (meshes, materials, textures). During cooking, these are saved as distinct secondary assets in the cache rather than being packed into an opaque monolith.

Dependencies are stored in the parent asset's `.import` file:

```json
{
  "guid": 12345678,
  "type": "dda828d0-0003-4c32-a110-1829a9c8f003",
  "dependencies": [
    {
      "name": "mesh/0",
      "guid": 87654321,
      "type": "a9c372f8-0001-4f6e-9821-3810a9c8f001",
      "path": "meshes/87654321.mesh"
    }
  ]
}
```

Cooker plugins use `AssetDependencyScope` to deterministically resolve GUIDs across re-imports and commit dependency metadata:

```cpp
AssetDependencyScope scope(metadata, source_path, caches_root);
AssetID mesh_id = scope.resolve(parent_guid, "mesh/0", MeshAsset::type, "meshes/87654321.mesh");
scope.commit();
```

### Recursive Loading & Unloading
When `AssetServer` loads a parent asset:
1. It queries the `AssetRegistry` for the parent's recorded dependencies.
2. It recursively invokes `load_asset()` for each dependency.
3. When the parent asset's reference count drops to zero, the server recursively decrements the reference counts of all dependencies, ensuring that shared sub-resources are released only when no longer referenced.

## API Interfaces

### AssetLoaderAPI
Defines how to take a binary file and construct an in-memory C++ asset.
```cpp
struct AssetLoaderAPI
{
    void (*configure)(AssetServer* manager, const JSON& options);
    void* (*load)(FileLoader* loader, FSPath path);
    void (*unload)(void* data);
    uint (*get_supported_extensions)(CString* extensions);
};
```

### AssetCookerAPI
Defines how to take a source file and produce a cooked binary file + metadata.
```cpp
struct AssetCookerAPI
{
    void (*configure)(AssetServer* manager, const JSON& options);
    bool (*process)(JSON& metadata, OSPath source_path, OSPath caches_root);
    uint (*get_supported_extensions)(CString* extensions);
};
```

### AssetSaverAPI
Defines how to serialize an in-memory asset back to disk.
```cpp
struct AssetSaverAPI
{
    void (*configure)(AssetServer* manager, const JSON& options);
    bool (*save)(const void* asset, OSPath path);
    uint (*get_supported_extensions)(CString* extensions);
};
```

### AssetPreviewAPI
Defines how to render offline preview scenes and generate thumbnail images.
```cpp
struct AssetPreviewAPI
{
    void (*configure)(AssetServer* manager, const JSON& options);
    PreviewTexture (*render_scene)(const PreviewScene& scene);
    Future<Path> (*generate_thumbnail)(const PreviewScene& scene, JSON& metadata);
};
```

## Practical Example: Defining & Loading an Asset

The following example defines a custom asset type with an inline loader, registers it with `AssetServer`, and initiates an asynchronous load:

```cpp
#include <Lyra/Assets/AMSServer.h>
#include <Lyra/FileSystem/VFSTypes.h>

using namespace lyra;

// 1. define the custom asset structure
struct TextFileAsset
{
    static constexpr CString     name = "TextFileAsset";
    static constexpr AssetTypeID type = make_uuid("11223344-5566-7788-99aa-bbccddeeff00");

    String content;

    static auto loader() -> AssetLoaderAPI
    {
        return AssetLoaderAPI{
            .configure = nullptr,
            .load = [](FileLoader* loader, FSPath path) -> void* {
                auto data = loader->read<char>(path);
                auto* asset = new TextFileAsset();
                asset->content = String(data.begin(), data.end());
                return asset;
            },
            .unload = [](void* data) {
                delete static_cast<TextFileAsset*>(data);
            },
            .get_supported_extensions = [](CString* exts) -> uint {
                if (exts) exts[0] = ".txt";
                return 1;
            }
        };
    }
};

// 2. initialize AMS and load the asset
void load_example()
{
    FileLoader vfs(FSLoader::NATIVE);
    vfs.mount("/", "C:/Project/Assets", 0);

    AMSDescriptor desc = {};
    desc.registry      = "C:/Project/Assets/Registry.toml";
    desc.loader.assets = &vfs;

    AssetServer ams(desc);
    ams.register_asset<TextFileAsset>();

    // queue asynchronous load
    AssetHandle<TextFileAsset> handle = ams.load_asset<TextFileAsset>("/data/intro.txt");

    // query pointer (returns nullptr while the background worker is loading)
    if (TextFileAsset* asset = ams.get_asset(handle)) {
        spdlog::info("Loaded: {}", asset->content);
    }

    // decrement reference count when finished
    ams.unload_asset(handle);
}
```
