# Job System

The **Job System** (`Lyra/JobSystem/`) is Lyra's unified multithreaded and heterogeneous execution runtime. Built on modern C++20 coroutines, a priority-based work-stealing worker pool, and non-blocking GPU completion monitoring, it orchestrates workloads across all available execution units—both CPU cores and GPU queues.

---

## Architecture & Header Organization

The Job System is structured into three clean, naturally separated layers:

```
Engine/Include/Lyra/JobSystem/
├── JobScheduler.h     # Layer 1: Core thread pool & work-stealing scheduler (zero coroutine dependency)
├── Task.h             # Layer 2: C++20 coroutine model (Task<T>, JobPromise<T>, Awaiters, Combinators)
├── Dispatch.h         # Layer 3: Unified execution entry point (dispatch CPU & GPU, GPUCompletionMonitor)
└── JobSystem.h        # Umbrella header (includes the above)
```

| Header | Role | Primary Types & Functions |
| :--- | :--- | :--- |
| [`JobScheduler.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/JobSystem/JobScheduler.h) | **Scheduling Engine** | `JobPriority`, `Job`, `JobSystemDescriptor`, `JobScheduler`, `assist_work` |
| [`Task.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/JobSystem/Task.h) | **Coroutine Model** | `Task<T>`, `JobPromise<T>`, `MainThreadAwaiter`, `WorkerAwaiter`, `BatchAwaiter`, `when_all`, `sync_wait`, `parallel_for` |
| [`Dispatch.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/JobSystem/Dispatch.h) | **Unified Dispatch** | `dispatch(CPU)`, `dispatch(GPU)`, `operator co_await(GPUFence)`, `GPUCompletionMonitor` |
| [`JobSystem.h`](file:///C:/Users/tcheng/source/repos/Lyra-Engine/Engine/Include/Lyra/JobSystem/JobSystem.h) | **Umbrella Header** | Includes `JobScheduler.h`, `Task.h`, and `Dispatch.h` |

---

## Core Scheduling Engine (`JobScheduler.h`)

### Priority Levels
Jobs and tasks are scheduled across three priority queues:

- **`JobPriority::HIGH`**: Frame-critical tasks (render preparation, culling, scene transform updates).
- **`JobPriority::NORMAL`**: Standard engine and gameplay computation (default).
- **`JobPriority::BACKGROUND`**: Asynchronous disk I/O, asset cooking, and background decompression.

### Work Stealing & Worker Assistance
When a thread blocks on synchronous completion via `sync_wait()`, it does not sleep. Instead, it runs `assist_work()` to execute pending jobs from the work-stealing Chase-Lev queues, maximizing CPU utilization and preventing deadlock when tasks depend on worker completion.

### Zero-Allocation Closures & Raw Jobs
* **Dedicated Closure Pool**: For lambdas with `sizeof(F) <= 128` bytes and standard alignment, the scheduler allocates from a thread-cached pool (`JobClosurePool`), avoiding heap churn. Closures larger than 128 bytes fall back to standard allocation.
* **Raw POD Jobs**: For minimal overhead, the scheduler accepts raw function pointer jobs:
  ```cpp
  JobScheduler::schedule(JobPriority::HIGH, Job{my_func, my_context});
  ```

### Main Thread Queue & External Pump
* **Main Thread Draining**: Operations requiring main-thread execution (WSI events, swapchain recreation) are queued via `JobScheduler::schedule_main(Job)`. The engine runtime pumps this queue once per frame via `JobScheduler::drain_main_thread(max_jobs)`.
* **External Completion Pump**: Non-CPU subsystems (such as the GPU completion monitor) register pump callbacks via `JobScheduler::register_external_pump(fn)`. Worker threads call this pump during `assist_work()` and idle loops to ensure fence completions trigger without dedicated polling threads.

---

## Coroutine Architecture (`Task.h`)

The primary abstraction for asynchronous flows is `Task<T>`, a C++20 coroutine task with symmetric transfer.

### Symmetric Transfer
Traditional coroutines can cause stack overflows when awaiting long chains of completed tasks. `Task<T>` implements symmetric transfer in `await_suspend`, returning the next coroutine handle directly to the runtime rather than resuming recursively on the current call stack.

### Thread Hopping
Coroutines can hop between execution contexts using built-in awaiters:

- `co_await JobScheduler::worker(priority)`: Resumes execution on a worker thread with the specified priority.
- `co_await JobScheduler::main_thread()`: Resumes execution on the main thread.

```cpp
Task<RenderData> prepare_render_data(AssetID id)
{
    // switch to worker thread for heavy parsing
    co_await JobScheduler::worker(JobPriority::HIGH);
    auto raw_data = parse_heavy_mesh(id);

    // switch back to main thread to interact with graphics context
    co_await JobScheduler::main_thread();
    auto gpu_mesh = upload_to_gpu(raw_data);

    co_return gpu_mesh;
}
```

### Producer-Side Promise (`JobPromise<T>`)
`JobPromise<T>` is a universal producer-side promise producing a `Task<T>` fulfilled by any producer (CPU worker thread, GPU monitor, IO pump) via `promise.set_value(...)`.

---

## Unified Dispatch (`Dispatch.h`)

The `dispatch(...)` function is overloaded in `namespace lyra` to provide a single, unified developer entry point for both CPU and GPU execution.

### CPU Dispatch
Dispatches an invocable to the worker pool and returns a `Task<ReturnType>`:

```cpp
// Typed CPU task
Task<int> compute_task = dispatch([]() -> int {
    return evaluate_physics_step();
});

// Awaiting result in another coroutine
int result = co_await compute_task;
```

### GPU Dispatch
Asynchronously records commands on a command encoder, submits them to the designated queue, and suspends the coroutine until the GPU fence signals:

```cpp
// Asynchronous compute dispatch
co_await dispatch(GPUQueueType::COMPUTE, [](GPUCommandEncoder& cmd) {
    cmd.dispatch_workgroups(64, 1, 1);
});

// Explicit device and high priority
co_await dispatch(device, GPUQueueType::TRANSFER, JobPriority::HIGH, [](GPUCommandEncoder& cmd) {
    cmd.copy_buffer_to_buffer(staging, dest, size);
});
```

### Direct Fence Awaiting (`co_await GPUFence`)
Any RHI `GPUFence` can be directly awaited:

```cpp
GPUFence fence = cmd.signal(...);
cmd.submit();

// suspends coroutine non-blockingly until fence is signaled on the GPU
co_await fence;
```

---

## Parallel Algorithms & Combinators

### `parallel_for`
Splits a contiguous range into chunked batches and dispatches them across worker threads with `HIGH` priority. The calling coroutine suspends until all batches complete:

```cpp
Task<void> update_particles(Vector<Particle>& particles, float dt)
{
    co_await parallel_for(0ull, particles.size(), [&](size_t i) {
        particles[i].position += particles[i].velocity * dt;
    });
}
```

### `when_all` (Heterogeneous Composition)
Fans out multiple concurrent tasks across CPU and GPU, suspending the caller until all tasks finish:

```cpp
Task<void> prepare_frame()
{
    auto cpu_culling = dispatch([] { run_frustum_culling(); });
    auto gpu_shadows = dispatch(GPUQueueType::COMPUTE, [](GPUCommandEncoder& cmd) {
        generate_virtual_shadow_maps(cmd);
    });

    // concurrently await both CPU and GPU operations
    co_await when_all(std::move(cpu_culling), std::move(gpu_shadows));
}
```

### `sync_wait`
Bridges synchronous code (application loops, unit tests) to coroutines:

```cpp
int main()
{
    JobScheduler::init();
    sync_wait(my_coroutine_task());
    JobScheduler::shutdown();
    return 0;
}
```

---

## Complete Example: Heterogeneous Simulation & Rendering

```cpp
#include <Lyra/JobSystem/JobSystem.h>
#include <iostream>

using namespace lyra;

Task<void> run_frame_pipeline(GPUDevice& device)
{
    // 1. Dispatch CPU physics computation
    auto cpu_task = dispatch([]() -> int {
        // heavy CPU computation
        return 42;
    });

    // 2. Dispatch concurrent GPU compute pass
    auto gpu_task = dispatch(device, GPUQueueType::COMPUTE, [](GPUCommandEncoder& cmd) {
        cmd.insert_debug_marker("GPU Compute Pass");
    });

    // 3. Concurrently await both CPU and GPU completions
    co_await when_all(std::move(cpu_task), std::move(gpu_task));

    // 4. Hop back to main thread for final present
    co_await JobScheduler::main_thread();
    std::cout << "Frame pipeline completed across CPU and GPU.\n";
}

int main()
{
    JobScheduler::init();

    auto device = RHI::get_current_device();
    sync_wait(run_frame_pipeline(device));

    JobScheduler::shutdown();
    return 0;
}
```
