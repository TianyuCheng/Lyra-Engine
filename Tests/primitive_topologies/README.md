# Primitive Topologies Test

## Description
This test verifies pipeline primitive topology configurations (`GPUPrimitiveTopology::TRIANGLE_LIST` and `GPUPrimitiveTopology::TRIANGLE_STRIP`) in both indexed and non-indexed draw modes across four isolated quadrants:

- **Top-Left (Indexed Triangle List)**: An indexed `TRIANGLE_LIST` pipeline draws a **Ruby Red** diamond emblem (4 vertices, 6 indices: `(0, 1, 2)` and `(0, 2, 3)`) centered on a Dark Slate base card.
- **Top-Right (Non-Indexed Triangle Strip)**: A `TRIANGLE_STRIP` pipeline draws an **Emerald Green** diamond emblem using 4 sequential vertices (`left`, `bottom`, `top`, `right`) with alternating triangle winding via `draw(4, 1, 0, 0)`.
- **Bottom-Left (Indexed Triangle Strip)**: An indexed `TRIANGLE_STRIP` pipeline draws a **Sapphire Blue** faceted hexagonal emblem using 6 sequential indices (`0..5`, forming 4 connected triangles) via `draw_indexed(6, 1, 0, 0, 0)`.
- **Bottom-Right (Non-Indexed Triangle Strip Ribbon)**: A `TRIANGLE_STRIP` pipeline draws an **Amber Gold** inverted chevron ribbon using 6 sequential vertices forming 4 connected strip triangles via `draw(6, 1, 30, 0)`.

If strip topologies, vertex winding alternation ($i \pmod 2$), or index-offset resolution are broken or unsupported, strips will produce degenerate triangles, inside-out flipped backfaces, distorted ribbons, or missing quadrants.

## Reference
![Reference](reference.png "Reference")
