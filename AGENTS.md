# AGENTS.md

Welcome to Lyra Engine. This document provides core architectural conventions, type system aliases, and guidelines for AI agents and human developers working across the codebase.

---

## 0. C++ coding style

Function signature:
use `auto function_name(...) -> return_type` in the header file when we need to align function signatures`.
use `return_type function_name(...)` in the cpp file when all functions are separate (no need to align).
The rule of switching which one to use is based on alignment.
use `auto` instead of `auto*`, because pointer is inferred.

Types:
use the lyra alias types if possible.
using `auto` instead of `auto*`.

## 0.5 HARD RULES

1. Don't do any hack just to make test pass (for example, relax pass criteria for softgpu, implement skybox in softgpu).

## 1. Lyra Common Type & Name Aliases (`Include/Lyra/Common/`)

Lyra defines unified type aliases in `Include/Lyra/Common/` to standardize standard library, Abseil, Apus, GLM, and vendor dependencies across the engine and plugins. Always prefer these aliases over raw standard library or third-party types.

### 1.1 Integer / Scalar Types ([`Stdint.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Stdint.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::ushort` | `uint16_t` | Unsigned 16-bit integer |
| `lyra::uint` | `uint32_t` | Unsigned 32-bit integer |
| `lyra::ulong` | `uint64_t` | Unsigned 64-bit integer |
| `lyra::usize` | `size_t` | Unsigned pointer-sized / container size integer |
| `lyra::isize` | `ptrdiff_t` | Signed pointer-sized / distance integer |

---

### 1.2 Strings & Character Types ([`String.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/String.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::String` | `std::string` | Standard dynamic string |
| `lyra::WString` | `std::wstring` | Wide string |
| `lyra::CString` | `const char*` | Null-terminated C-style string pointer |
| `lyra::StringView` | `std::string_view` | Non-owning string view |
| `lyra::LPCWSTR` | `const wchar_t*` | Windows compatibility pointer |

#### String Utilities:
- `to_string(const String&)` / `to_string(const WString&, locale)`: String conversions.
- `to_wstring(CString)` / `to_wstring(const String&)`: Narrow to wide string conversions.

---

### 1.3 Path Types ([`Path.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Path.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Path` | `std::filesystem::path` | Cross-platform filesystem path |
| `lyra::OSPath` | `const std::filesystem::path::value_type*` | OS-native path string (`const wchar_t*` on Windows, `const char*` on POSIX) |
| `lyra::FSPath` | `lyra::CString` (`const char*`) | Virtual filesystem (VFS) UTF-8 path |
| `lyra::fs` | `namespace std::filesystem` | Convenient alias for standard filesystem |

---

### 1.4 Collections & Containers ([`Collections.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Collections.h))
Always prefer Lyra collection aliases. Note that associative containers use **Abseil** and **Apus** for performance:

| Lyra Alias | Underlying Type | Category / Characteristics |
| :--- | :--- | :--- |
| `lyra::Vector<T>` | `std::vector<T...>` | Dynamic contiguous array |
| `lyra::Array<T, N>` | `std::array<T, N>` | Fixed-size contiguous array |
| `lyra::SmallVector<T, N>` | `apus::small_vector<T, N>` | Small-buffer optimized vector |
| `lyra::Deque<T>` | `std::deque<T...>` | Double-ended queue |
| `lyra::List<T>` | `std::list<T...>` | Doubly linked list |
| `lyra::FList<T>` | `std::forward_list<T...>` | Singly linked list |
| `lyra::Stack<T>` | `std::stack<T...>` | LIFO stack adapter |
| `lyra::RingBuffer<T>` | `apus::ring_buffer<T...>` | Circular buffer |
| `lyra::Slotmap<T>` | `apus::slot_map<T...>` | Stable index generational slot map |
| `lyra::HashSet<T>` | `absl::flat_hash_set<T...>` | High-performance cache-friendly hash set |
| `lyra::HashMap<K, V>` | `absl::flat_hash_map<K, V...>` | High-performance cache-friendly hash map |
| `lyra::TreeMap<K, V>` | `absl::btree_map<K, V...>` | Cache-efficient ordered B-tree map |
| `lyra::MultiMap<K, V>` | `absl::btree_multimap<K, V...>` | Ordered B-tree multimap |
| `lyra::Variant<T...>` | `std::variant<T...>` | Type-safe union |
| `lyra::Optional<T>` | `std::optional<T...>` | Optional value container |
| `lyra::InitList<T>` | `std::initializer_list<T...>` | Initializer list |
| `lyra::TypedView<T...>` | `lyra::detail::typed_view<T...>` | Type-erased or typed contiguous span |
| `lyra::Blackboard` | `lyra::detail::Blackboard` | Dynamic heterogeneous property storage |

---

### 1.5 Smart Pointers & Ownership ([`Pointer.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Pointer.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Own<T, Deleter>` | `std::unique_ptr<T, Deleter>` | Exclusive ownership pointer |
| `lyra::Ref<T>` | `std::shared_ptr<T>` | Reference-counted shared ownership pointer |
| `lyra::OwnedResource<T>` | `std::unique_ptr<T, Destroyer<T>>` | Resource calling `pointer->destroy()` on destruction |
| `lyra::make_ref<T>(...)` | `std::make_shared<T>(...)` | Factory helper for `Ref<T>` |
| `lyra::make_own<T>(...)` | `std::make_unique<T>(...)` | Factory helper for `Own<T>` |

---

### 1.6 Configuration & Serialization ([`Config.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Config.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::JSON` | `nlohmann::json` | JSON DOM representation |
| `lyra::TOML` | `toml::table` | toml++ table representation |

---

### 1.7 Functional & Callables ([`Function.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Function.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Function<Sig>` | `absl::AnyInvocable<Sig>` | Move-only generic invocable object |
| `lyra::FunctionRef<Sig>` | `absl::FunctionRef<Sig>` | Non-owning, lightweight function reference |
| `lyra::Delegate<Sig>` | `etl::delegate<Sig>` | Embedded Template Library delegate |
| `lyra::execute(fn)` | Template helper | Inlines and executes a callable immediately |
| `lyra::execute_once(fn)` | Template helper | Thread-safe single execution helper |

---

### 1.8 Asynchronous & Concurrency ([`Promise.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Promise.h), [`Sync.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Sync.h), & [`Coroutine.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Coroutine.h))
| Lyra Alias / Helper | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Promise<T>` | `std::promise<T>` | Standard promise |
| `lyra::Future<T>` | `std::future<T>` | Standard future |
| `lyra::Mutex` | `std::mutex` | Mutual exclusion lock |
| `lyra::RecursiveMutex` | `std::recursive_mutex` | Recursive mutual exclusion lock |
| `lyra::SharedMutex` | `std::shared_mutex` | Shared / reader-writer mutex |
| `lyra::LockGuard<M>` | `std::lock_guard<M>` | Scoped exclusive lock |
| `lyra::UniqueLock<M>` | `std::unique_lock<M>` | Scoped unique / deferred lock |
| `lyra::SharedLock<M>` | `std::shared_lock<M>` | Scoped shared read lock |
| `lyra::ConditionVariable` | `std::condition_variable` | Condition variable for `UniqueLock<Mutex>` |
| `lyra::ConditionVariableAny` | `std::condition_variable_any` | Universal condition variable |
| `lyra::Atomic<T>` | `std::atomic<T>` | Atomic type wrapper |
| `lyra::AtomicFlag` | `std::atomic_flag` | Atomic boolean flag |
| `lyra::MemoryOrder` | `std::memory_order` | Atomic memory ordering enumeration |
| `lyra::CoroutineHandle<P>` | `std::coroutine_handle<P>` | Strongly typed or erased coroutine handle |
| `lyra::SuspendAlways` | `std::suspend_always` | Coroutine unconditional suspension awaiter |
| `lyra::SuspendNever` | `std::suspend_never` | Coroutine no-op suspension awaiter |
| `lyra::with_lock(mtx, fn)` | Helper template | Inlines execution while holding an exclusive lock |
| `lyra::with_shared_lock(mtx, fn)` | Helper template | Inlines execution while holding a shared read lock |

> [!IMPORTANT]
> **Synchronization & Concurrency Rule**: Always prefer Lyra concurrency and synchronization aliases (`lyra::Mutex`, `lyra::SharedMutex`, `lyra::LockGuard`, `lyra::UniqueLock`, `lyra::ConditionVariable`, `lyra::Atomic<T>`, `lyra::CoroutineHandle<P>`, etc.) from [`Sync.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Sync.h) and [`Coroutine.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/Coroutine.h) instead of raw `std::` concurrency types.

---

### 1.9 Type Traits & Metaprogramming ([`TypeTraits.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Utilities/TypeTraits.h))
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Conditional<B, T, F>` | `std::conditional_t<B, T, F>` | Selects type `T` if `B` is true, otherwise `F` |
| `lyra::EnableIf<B, T>` | `std::enable_if_t<B, T>` | SFINAE type enabler |
| `lyra::RemoveCvRef<T>` | `std::remove_cvref_t<T>` | Strips `const`, `volatile`, and reference qualifiers |
| `lyra::Decay<T>` | `std::decay_t<T>` | Applies decay type transformations |

---

### 1.10 Math Primitives ([`Math.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Math.h))
Lyra wraps GLM with Depth $[0, 1]$ and Radians enabled:
| Lyra Alias | Underlying Type | Description |
| :--- | :--- | :--- |
| `lyra::Vector2` | `glm::mediump_vec2` | 2D float vector |
| `lyra::Vector3` | `glm::mediump_vec3` | 3D float vector |
| `lyra::Vector4` | `glm::mediump_vec4` | 4D float vector |
| `lyra::Quaternion` | `glm::quat` | Quaternion rotation |
| `lyra::Matrix2x2` .. `Matrix4x4` | `glm::mediump_matNxM` | Column-major transformation matrices |

---

### 1.10 Identification & Handle Types
| Type | Header | Description |
| :--- | :--- | :--- |
| `lyra::GUID` | [`GUID.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/GUID.h) | `uint64_t` compact runtime identifier. Supports `random_guid()`, `deterministic_guid(parent, name)`, `probe_guid()`. |
| `lyra::UUID` | [`UUID.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/UUID.h) | 128-bit global identifier (`ulong hi, ulong lo`). Compile-time `make_uuid("...")`, runtime `parse_uuid()`, `to_string()`. Used for `AssetTypeID`. |
| `lyra::Handle<T, Tag>` | [`Handle.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Handle.h) | Strongly typed index/handle wrapper. |

---

### 1.11 Logging & Diagnostics
| Type / Macro | Header | Description |
| :--- | :--- | :--- |
| `lyra::Logger` | [`Logger.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Logger.h) | `std::shared_ptr<spdlog::logger>` |
| `lyra::LogLevel` | [`Logger.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Logger.h) | `spdlog::level::level_enum` |
| `FORCE_INLINE` | [`Macros.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Macros.h) | Compiler-specific force inline |
| `DEBUG_BREAK()` | [`Macros.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Macros.h) | Cross-platform debugger breakpoint |
| `UNIMPLEMENTED(msg)` | [`Macros.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Include/Lyra/Common/Macros.h) | Logs error and triggers debugger breakpoint |

---

## 2. Asset Management System (AMS) Conventions

- **Asset IDs & Handles**:
  - `lyra::AssetID` is `lyra::GUID` (`uint64_t`).
  - `lyra::AssetTypeID` is `lyra::UUID` (128-bit identifier).
  - `lyra::RawAssetHandle` holds a single `AssetID guid` field (`guid != 0` for validity; no backward-compatible aliases).
- **Secondary / Generated Assets (Dependencies)**:
  - Do **not** use the term "SubAsset". Use **dependency / dependencies** consistently.
  - Secondary cooked assets (meshes, materials, textures generated from a container like glTF/OBJ/STL) are recorded in the parent's `.import` file under the `"dependencies"` array:
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
  - Cooker plugins must use `struct AssetDependencyScope` ([`AMSUtils.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/Assets/AMSUtils.h)) to resolve stable, deterministic GUIDs and write dependency metadata.
  - Always use `struct AssetDependencyScope` (not `class`). Its method implementations are placed in [`Engine/Source/Assets/AMSUtils.cpp`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Source/Assets/AMSUtils.cpp).

---

## 3. Code Style & Namespace Conventions

- **C++ Source Files (`.cpp`)**:
  - Do **not** wrap method implementations inside `namespace lyra { ... }` blocks with an extra indentation level.
  - Instead, use `using namespace lyra;` (or fully qualified identifiers `lyra::ClassName::method`) at file scope and define methods with **zero indentation**:
    ```cpp
    #include <Lyra/Assets/AMSUtils.h>

    using namespace lyra;

    AssetDependencyScope::AssetDependencyScope(JSON& metadata, const Path& source_path, const Path& caches_root)
        : metadata(metadata), source_path(source_path), caches_root(caches_root)
    {
        // implementation without namespace indentation
    }
    ```

- **Comments**:
  - Prefer lowercase comments (e.g. `// resolve stable id across re-imports` rather than `// Resolve stable id across re-imports`).

- **`auto` & Code Alignment**:
  - `auto` can be used for variable declarations, as well as `auto` with trailing return types for function declarations (`auto func(...) -> ReturnType`).
  - The function trailing return type rule only applies to headers (`.h`), and it only applies to cases where we have consecutive declarations next to each other. This rule is introduced ONLY for alignment purpose.
  - In source files (`.cpp`), use standard leading return types (`ReturnType ClassName::func(...)`).
  - For return types that are less than or equal to 4 characters (especially `void`, `bool`, etc.), this `auto` rule does **not** apply. Standard return types should be retained without trailing `auto -> ...`. Exercise thoughtful judgement when transforming function signatures.

---

## 4. Architecture & Design Principles

- **No Backward Compatibility Constraints**:
  - There is **no backward compatibility concern** in Lyra.
  - When designing or refactoring APIs, types, or systems, prioritize the cleanest, most optimal, and modern architecture directly.
  - Do **not** create or retain legacy adapters, compatibility aliases, deprecated forwarders, or obsolete shims. Clean breakage and refactoring to the optimal design is always preferred.

- **Layer-Specific Type Prefixes**:
  - The `GPU*` prefix is strictly reserved for the **RHI (Graphics) layer** (`Include/Lyra/Graphics/`, e.g., `GPUBuffer`, `GPUTexture`, `GPUCommandEncoder`, `GPUBLAS`).
  - Higher engine layers (such as the **Rendering layer** in `Include/Lyra/Rendering/`) must **not** use the `GPU*` prefix. Use natural domain names (e.g., `MeshGeometryEntry`, `GeometryLOD`, `IndexedGeometry`, `MeshletGeometry`) or layer-appropriate prefixes.

---

## 5. Shader Authoring & Resource Binding Conventions (`.slang`)

- **No Backend-Specific Attributes**:
  - Shaders must **never** use API- or backend-specific binding attributes such as `[[vk::binding(...)]]`, `[[vk::push_constant]]`, `[[vk::image_format(...)]]`, or HLSL `: register(b0, space0)` syntax.
  - Lyra targets Vulkan (SPIR-V), Direct3D 12 (DXIL), and Metal (MSL) from a single unified Slang source. Backend-specific syntax breaks portability and reflection across targets.

- **Use Lyra Standard Annotations (`import lyra;`)**:
  - Always `import lyra;` in Slang shaders requiring resource bindings or immediates.
  - **Immediates / Push Constants**:
    `[[lyra::immediates]] ConstantBuffer<T> name;`
    Automatically lowered to `[[vk::push_constant]]` on SPIR-V, `register(b0, space1000)` on DXIL, and the reserved root buffer on Metal.
  - **Explicit Bind Groups / Sets**:
    `[[lyra::group(N)]] ParameterBlock<T> name;`
    Assigns the ParameterBlock to bind group index `N`.
  - **Explicit Bindings**:
    `[[lyra::binding(slot)]]` (single slot) or `[[lyra::binding(slot, group)]]` (explicit slot and group).
  - **Dynamic Uniforms**:
    `[lyra::dynamic] ConstantBuffer<T> name;`

---

## 6. Build, Test & Workflow Guidelines (`Justfile`)

Agents and developers working on Lyra must use [`Justfile`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Justfile) (`just`) rather than running underlying tools (`cmake`, `ninja`, `python Tools/Scripts/...`, `clang-format`, etc.) directly.

### 6.1 Available `just` Recipes
| Command | Description |
| :--- | :--- |
| `just list` | List all available CMake presets |
| `just config <generator> [preset="debug"]` | Configure project generator (e.g. `ninja`, `vs2022`) and preset (`debug`, `release`) |
| `just switch <preset>` | Switch active configuration preset |
| `just build [target="all"]` | Build all targets or a specific target (e.g. `just build editor`, `just build testkit`) |
| `just run [target="all"] [args...]` | Build and run a target executable (e.g. `just run editor`, `just run player`) |
| `just test [target="all"]` | Build test suite and run all or filtered tests (e.g. `just test`, `just test unit-tests`, `just test rhi-tests`, `just test <filter>`) |
| `just format` | Automatically format all C++ code using `clang-format` |
| `just format-check` | Check code formatting compliance without modifying files |
| `just clean` | Clean all build products (requires confirmation) |

> [!IMPORTANT]
> **Workflow Rule**: Always prefer running tasks via `just <recipe>` instead of invoking low-level build commands directly. For testing, always use `just test` (or `just test <target>`) to ensure both unit tests and RHI regression tests with perceptual verification are executed.
