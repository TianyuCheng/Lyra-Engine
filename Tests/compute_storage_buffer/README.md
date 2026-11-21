# Compute Storage Buffer Test

## Description
This test verifies compute shader interaction with GPU structured storage buffers (`StructuredBuffer<T>`), 2D storage image writes (`RWTexture2D<float4>`), workgroup dispatching, and readback.

The compute shader fetches geometric emblem properties from a structured buffer containing an array of `QuadrantBadge` records (colors, quadrant centers, radii, and shape types) to render four distinct procedural emblems:

- **Top-Left (Quadrant 0 - Concentric Diamond)**: Multi-tier diamond badge rendered via L1 Manhattan distance norm using storage buffer colors in **Mint** (`#00E676`), **Emerald** (`#00C754`), and **Dark Forest** (`#1C5E21`).
- **Top-Right (Quadrant 1 - Radiant Star)**: Polar harmonic modulation generating a 4-pointed radiant star with storage buffer colors in **Electric Blue** (`#2979FF`) and **Deep Sapphire** (`#1466BF`).
- **Bottom-Left (Quadrant 2 - Stacked Chevrons)**: Piecewise linear coordinate transformations creating stacked chevron emblems with storage buffer colors in **Amber Gold** (`#FFD600`) and **Sunset Coral** (`#FF5722`).
- **Bottom-Right (Quadrant 3 - Hexagonal Badge)**: Exact 2D hexagonal distance norm producing a concentric hexagon badge with storage buffer colors in **Bright Ruby** (`#FF1744`) and **Dark Magenta** (`#C2175B`).

If storage buffer creation, structured buffer memory alignment, binding table translation, or compute buffer indexing is broken, the badges will be missing, distorted, or incorrectly colored.

## Reference
![Reference](reference.png "Reference")
