# Virtual File System (VFS)

The **Virtual File System** (`Lyra/FileSystem/`) decouples asset loading and engine operations from physical disk layouts. It unifies physical directories, mod overlays, and compressed package archives under a single virtual directory hierarchy.

## Core Concepts

### Virtual vs. Native Paths
Lyra strictly differentiates between native OS paths and virtual paths:

- **`OSPath` / `Path`**: Standard filesystem paths (`std::filesystem::path`), used by cookers, file watchers, and tools that interact directly with the operating system.
- **`FSPath`**: UTF-8 null-terminated virtual path strings (e.g., `/assets/textures/wood.ktx2`), used throughout runtime asset loaders and engine queries.

### Mount Points and Priority Overlays
The virtual filesystem supports mounting both OS directories and archive packages to virtual mount points. Each mount is assigned an integer priority:

```cpp
FileLoader loader(FSLoader::NATIVE);

// mount base game assets at low priority
MountHandle base_mount = loader.mount("/assets", "C:/Games/Lyra/BaseData", 0);

// mount patch / mod directory at higher priority
MountHandle patch_mount = loader.mount("/assets", "C:/Games/Lyra/Patches", 10);
```

When resolving a virtual path (such as `/assets/scripts/player.json`), the system searches mounts in descending priority order. Higher-priority mounts shadow files in lower-priority mounts, allowing game updates and user modifications without altering base assets.

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

    // 2. mount base directory to virtual root
    MountHandle mount = vfs.mount("/", "C:/Games/LyraEngine/Assets", 0);

    // 3. check file existence
    if (vfs.exists("/shaders/mesh.spv")) {
        // query file size
        size_t file_size = vfs.size("/shaders/mesh.spv");

        // bulk-read binary data directly into a vector
        Vector<uint8_t> shader_bytes = vfs.read<uint8_t>("/shaders/mesh.spv");
        std::cout << "Read " << shader_bytes.size() << " bytes.\n";
    }

    // 4. unmount when done
    vfs.unmount(mount);
}
```
