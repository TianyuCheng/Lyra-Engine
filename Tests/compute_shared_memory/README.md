# Compute Shared Memory Test

## Description
This test verifies compute shader execution utilizing workgroup shared memory (`groupshared` / Local Data Share / threadgroup memory) and execution synchronization barriers (`GroupMemoryBarrierWithGroupSync()`).

The compute shader operates with a 32x32 threadgroup layout (`[numthreads(32, 32, 1)]`):
1. **Phase 1 (Tile Population)**: Each thread computes the initial color for its global pixel location within the four quadrant cards and deposits it into the threadgroup shared array `s_tile[32][32]`.
2. **Phase 2 (Barrier Synchronization)**: An execution and memory fence (`GroupMemoryBarrierWithGroupSync()`) synchronizes all 1,024 threads in the threadgroup, ensuring all shared memory writes are complete.
3. **Phase 3 (Shared Memory Filtering)**: Each thread executes an intra-tile 3x3 box blur filtering pass reading strictly from neighbor texels in `s_tile` with clamped local threadgroup boundaries before writing the filtered result to the output 2D storage image (`RWTexture2D<float4>`).

The rendered badges in each quadrant:
- **Top-Left (Quadrant 0)**: Blurred concentric diamond emblem in **Mint** (`#00E676`), **Emerald** (`#00C754`), and **Dark Forest** (`#1C5E21`).
- **Top-Right (Quadrant 1)**: Blurred 4-pointed radiant star in **Electric Blue** (`#2979FF`) and **Deep Sapphire** (`#1466BF`).
- **Bottom-Left (Quadrant 2)**: Blurred stacked chevron emblems in **Amber Gold** (`#FFD600`) and **Sunset Coral** (`#FF5722`).
- **Bottom-Right (Quadrant 3)**: Blurred concentric hexagon badge in **Bright Ruby** (`#FF1744`) and **Dark Magenta** (`#C2175B`).

If workgroup shared memory allocation, threadgroup barrier synchronization, or local thread coordinate indexing is broken, the output will suffer visible tearing, tile edge corruption, or severe perceptual mismatch.

## Reference
![Reference](reference.png "Reference")
