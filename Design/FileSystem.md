# Virtual File System (VFS)

The **Virtual File System** (`Lyra/FileSystem/`) decouples asset loading and engine operations from physical disk layouts. It unifies physical directories, mod overlays, and compressed package archives under a single virtual directory hierarchy.

## Core Concepts

### Virtual vs. Native Paths & Protocol Schemes
Lyra strictly differentiates between native OS paths and protocol-qualified virtual paths:

- **`OSPath` / `Path`**: Standard filesystem paths (`std::filesystem::path`), used by cookers, file watchers, and tools that interact directly with the operating system.
- **`FSPath` / `VFSPath`**: Protocol-qualified UTF-8 virtual path strings. Schemes are **strictly required**; bare relative paths without a protocol scheme are rejected.
  - `sys://`: Dedicated domain for engine assets and shaders (e.g., `sys://shaders/pbr.hlsl`).
  - `app://`: Dedicated domain for game project assets (e.g., `app://textures/wood.png`).

### Mount Points, Domains, and Priority Overlays
The virtual filesystem supports mounting OS directories and package archives into isolated protocol domains. Each mount specifies a protocol scheme/subpath and an integer priority:

```cpp
FileLoader loader(FSLoader::NATIVE);

// mount engine assets under the sys:// domain
MountHandle engine_mount = loader.mount("sys://", "C:/Lyra/Engine/Assets", 0);

// mount base project assets under the app:// domain
MountHandle base_mount   = loader.mount("app://", "C:/MyGame/Assets", 0);

// mount patch directory at higher priority within app://
MountHandle patch_mount  = loader.mount("app://", "C:/MyGame/Patches", 10);
```

### Domain Isolation Policy
Protocol domains are strictly isolated:
- Queries targeting `sys://...` only resolve against `sys://` mounts.
- Queries targeting `app://...` only resolve against `app://` mounts and **never** fall back to `sys://`.
- Bare relative paths (e.g., `textures/wood.png` or `/assets/...`) without a scheme are rejected.
- Higher-priority mounts within the same domain shadow lower-priority mounts.

## Plugin Interfaces

File system implementations are exposed as modular plugins through `FileLoaderAPI` and `FilePackerAPI`:

### `FileLoaderAPI`
Defines stream reading, queries, and mount handling:

```cpp
struct FileLoaderAPI
{
    CString (*get_api_name)();

    bool (*create_loader)(FileLoaderHandle& loader);
    bool (*delete_loader)(FileLoaderHandle loader);

    size_t (*sizeof_file)(FileLoaderHandle loader, FSPath path);
    bool   (*exists_file)(FileLoaderHandle loader, FSPath path);

    bool   (*open_file)(FileLoaderHandle loader, FileHandle& handle, FSPath path);
    void   (*close_file)(FileLoaderHandle loader, FileHandle handle);
    bool   (*read_file)(FileLoaderHandle loader, FileHandle handle, void* buffer, size_t size, size_t& bytes_read);
    bool   (*seek_file)(FileLoaderHandle loader, FileHandle handle, int64_t offset);
    bool   (*read_whole_file)(FileLoaderHandle loader, FSPath path, void* data);

    bool   (*mount)(FileLoaderHandle loader, MountHandle& handle, FSPath vpath, OSPath path, uint priority);
    bool   (*unmount)(FileLoaderHandle loader, MountHandle handle);
};
```

### `FilePackerAPI`
Defines archive writing and package generation:

```cpp
struct FilePackerAPI
{
    CString (*get_api_name)();

    bool (*create_packer)(FilePackerHandle& packer, OSPath path);
    void (*delete_packer)(FilePackerHandle packer);

    bool (*write)(FilePackerHandle packer, FSPath path, void* buffer, size_t size);
};
```

## Runtime Wrappers (`FileLoader` & `FilePacker`)

Engine subsystems interact with VFS through high-level C++ wrappers:

- **`FileLoader`**: Provides stream-based file reading (`open()`, `read()`, `seek()`, `close()`), existence checks (`exists()`), and convenient bulk reading (`read<uint8_t>(vpath)`).
- **`FilePacker`**: Writes binary buffers or disk files into an archive target.

During runtime, the `AssetServer` supplies an active `FileLoader*` to each asset loader, ensuring asset parsing code remains completely decoupled from physical archive storage or disk layout.

## Practical Example: Mounting Directories and Reading Files

The following example shows initializing a `FileLoader`, mounting an assets directory with priority, checking for file existence, and reading file contents into memory:

```cpp
#include <Lyra/FileSystem/VFSTypes.h>
#include <iostream>

using namespace lyra;

void vfs_example()
{
    // 1. initialize native filesystem loader
    FileLoader vfs(FSLoader::NATIVE);

    // 2. mount project directory to app:// domain
    MountHandle mount = vfs.mount("app://", "C:/Games/MyProject/Assets", 0);

    // 3. check file existence with mandatory scheme
    if (vfs.exists("app://textures/wood.png")) {
        // query file size
        size_t file_size = vfs.size("app://textures/wood.png");

        // bulk-read binary data directly into a vector
        Vector<uint8_t> texture_bytes = vfs.read<uint8_t>("app://textures/wood.png");
        std::cout << "Read " << texture_bytes.size() << " bytes.\n";
    }

    // 4. unmount when done
    vfs.unmount(mount);
}
```
