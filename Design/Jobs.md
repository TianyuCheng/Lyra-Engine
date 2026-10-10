# Job System

The **Job System** (`Lyra/JobSystem/`) provides a multithreaded task scheduler built on C++20 coroutines and a priority-based worker thread pool. It handles frame-critical parallel tasks (such as frustum culling and scene hierarchy evaluation), background operations (asset cooking), and seamless switching between worker threads and the main thread.

## Core Concepts

### Priority Levels
Jobs and tasks are scheduled across three priority queues:

- **`JobPriority::HIGH`**: Frame-critical tasks (render preparation, culling, scene transform updates).
- **`JobPriority::NORMAL`**: Standard engine and gameplay computation (default).
- **`JobPriority::BACKGROUND`**: Asynchronous disk I/O, asset cooking, and background decompression.

### Work Stealing & Worker Assistance
When a thread blocks on synchronous completion via `sync_wait()`, it does not sleep. Instead, it runs `assist_work()` to execute pending jobs from the scheduler queues, maximizing CPU utilization and preventing deadlock when tasks depend on worker completion.

## Coroutine Architecture (`Task<T>`)

The primary abstraction for asynchronous flows is `Task<T>` (`Lyra/JobSystem/Tasks.h`), a C++20 coroutine task with symmetric transfer.

### Symmetric Transfer
Traditional coroutines can cause stack overflows when awaiting long chains of completed tasks. `Task<T>` implements symmetric transfer in `await_suspend`, returning the next coroutine handle directly to the runtime rather than resuming recursively on the current call stack.

### Thread Hopping
Coroutines can hop between execution contexts using built-in awaiters:

- `co_await JobScheduler::worker(priority)`: Resumes execution on a worker thread with the specified priority.
- `co_await JobScheduler::main_thread()`: Resumes execution on the main thread (required for OS window operations or graphics present calls).

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

## Parallel Algorithms

### `parallel_for`
Splits a contiguous range into chunked batches and dispatches them across worker threads with `HIGH` priority. The calling coroutine suspends until all batches call `finish()` on the internal `BatchAwaiter`:

```cpp
Task<void> update_particles(Vector<Particle>& particles)
{
    co_await parallel_for(0ull, particles.size(), [&](size_t i) {
        particles[i].position += particles[i].velocity * dt;
    });
}
```

### `when_all`
Fans out multiple concurrent tasks and suspends the caller until all tasks complete:

```cpp
Task<void> load_scene_resources()
{
    co_await when_all(
        load_terrain_mesh(),
        load_environment_map(),
        load_actor_models()
    );
}
```

### `sync_wait`
Bridges synchronous code (such as application lifecycle loops or test suites) to coroutines. It starts the task and pumps the main thread work queue and worker assist loops until the task finishes:

```cpp
Task<int> async_compute();

int main()
{
    JobScheduler::init();
    int result = sync_wait(async_compute());
    JobScheduler::shutdown();
    return result;
}
```

## Low-Overhead Dispatch

### Zero-Allocation Closures
Scheduling fine-grained lambdas often introduces severe heap allocation overhead. The scheduler uses a dedicated closure pool (`JobClosurePool`) for callables with `sizeof(F) <= 128` bytes and standard alignment. Closures larger than 128 bytes fall back to dynamic allocation.

### Raw POD Jobs
For minimal overhead, the scheduler accepts raw function pointer jobs:

```cpp
struct Job
{
    void (*fn)(void*) = nullptr;
    void* data        = nullptr;

    void execute() const noexcept;
};

JobScheduler::schedule(JobPriority::HIGH, Job{my_func, my_context});
```

### Main Thread Draining
Non-thread-safe engine operations can be queued to the main thread via `JobScheduler::schedule_main(Job job)`. The engine runtime pumps this queue once per frame by invoking `JobScheduler::drain_main_thread(max_jobs)`.

## Complete Example: Parallel Simulation with Coroutines

The following example initializes the scheduler, launches a coroutine task via `sync_wait()`, parallelizes computation over worker threads using `parallel_for()`, and switches back to the main thread:

```cpp
#include <Lyra/JobSystem/JobSystem.h>
#include <iostream>

using namespace lyra;

struct Particle
{
    Vector3 position;
    Vector3 velocity;
};

Task<void> simulate_particles(Vector<Particle>& particles, float dt)
{
    // 1. hop to worker pool for compute
    co_await JobScheduler::worker(JobPriority::HIGH);

    // 2. run parallel loop chunked across worker threads
    co_await parallel_for(0ull, particles.size(), [&](size_t i) {
        particles[i].position += particles[i].velocity * dt;
    });

    // 3. hop back to the main thread
    co_await JobScheduler::main_thread();
    std::cout << "Simulation batch finished on main thread.\n";
}

int main()
{
    JobScheduler::init();

    Vector<Particle> particles(100'000, {Vector3(0.0f), Vector3(1.0f, 0.0f, 0.0f)});

    // bridge synchronous main thread to coroutine
    sync_wait(simulate_particles(particles, 0.016f));

    JobScheduler::shutdown();
    return 0;
}
```
