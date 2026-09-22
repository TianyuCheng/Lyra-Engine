# Asset Management System (AMS)

The **Asset Management System (AMS)** is responsible for the lifecycle of engine assets, including their discovery, preprocessing (cooking), asynchronous loading, and reference-counted memory management.

## Core Architecture

The system is built around three primary pillars:

1.  **AssetServer**: The central orchestrator that manages loading requests, maintains the active asset cache, and handles background worker threads.
2.  **AssetRegistry**: A persistent database (Binary or TOML) that maps Globally Unique Identifiers (GUIDs) to virtual file paths and maintains the asset dependency graph.
3.  **Processors (Loaders & Cookers)**: Plugin-based interfaces that define how specific asset types (Textures, Models, Materials) are processed from raw source files and loaded into engine-ready memory structures.

## Asset Lifecycle

### 1. Importing & Cooking
Before a raw file (e.g., `.png`, `.obj`) can be used by the engine, it must be "cooked" into an optimized format.
- The `AssetCookerAPI` processes source files and generates engine-ready binary blobs.
- It also generates a `.import` metadata file (JSON) which contains the asset's GUID, type, and its list of dependencies.
- The `AssetServer` registers these findings in the `AssetRegistry`.

### 2. Handles and Reference Counting
Assets are accessed via type-safe `AssetHandle<T>`.
- Loading an asset returns a handle and increments an internal reference counter.
- Unloading an asset decrements the counter.
- When the counter reaches zero, the asset is marked as a candidate for purging.
- `AssetServer::purge()` is called periodically (typically at scene transitions) to actually free the memory of unused assets.

### 3. Asynchronous Loading
All asset loading is asynchronous by default.
- When `load_asset()` is called, the server checks if the asset is already in memory.
- If not, it allocates a record and queues a loading task to a background thread pool.
- The `get_asset()` call returns `nullptr` until the background task completes and populates the data.

## Asset Dependencies

The AMS supports a declarative dependency system that allows for automatic recursive loading.

### Dependency Discovery
During the cooking process, the cooker identifies external references (e.g., a Material referencing Textures). These references are stored as an array of GUIDs in the asset's `.import` metadata and subsequently cached in the `AssetRegistry`.

### Automatic Recursive Loading
When the `AssetServer` begins loading a "parent" asset:
1. It queries the `AssetRegistry` for the parent's dependencies.
2. It recursively calls `load_asset()` for every child in the dependency list.
3. This ensures that all required sub-resources are either already in memory or are being loaded in parallel with the parent.

### Recursive Unloading
When the parent asset's reference count reaches zero, the server automatically decrements the reference counts of all its dependencies. This ensures that sub-resources are cleaned up when they are no longer needed by any parent asset.

## API Interfaces

### AssetLoaderAPI
Defines how to take a binary file and construct a C++ object.
```cpp
struct AssetLoaderAPI {
    void (*configure)(AssetServer* manager, const JSON& options);
    void* (*load)(FileLoader* loader, FSPath path);
    void (*unload)(void* data);
    uint (*get_supported_extensions)(CString* extensions);
};
```

### AssetCookerAPI
Defines how to take a source file and produce a binary file + metadata.
```cpp
struct AssetCookerAPI {
    void (*configure)(AssetServer* manager, const JSON& options);
    bool (*process)(JSON& metadata, OSPath source_path, OSPath caches_root);
    uint (*get_supported_extensions)(CString* extensions);
};
```
