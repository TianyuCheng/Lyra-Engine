# Fragment Discard Test

## Description
This test verifies fragment shader conditional `discard` execution in the rasterization pipeline.

Four quads are rendered across four quadrants over a dark navy slate clear background (`#141A2E`), each demonstrating a different procedural cutout pattern via fragment `discard`:

- **Top-Left (Circular Cutout)**: Fragments within radius $r < 0.316$ ($\text{dot}(uv - 0.5, uv - 0.5) < 0.10$) are discarded, carving a clean circular hole in a Coral Red card.
- **Top-Right (Checkerboard Cutout)**: Alternating $6 \times 6$ checkerboard cells are discarded, rendering a perforated Mint Green grid.
- **Bottom-Left (Horizontal Stripes Cutout)**: Alternating horizontal bands ($\text{frac}(uv.y \times 6.0) < 0.45$) are discarded, creating a slatted Sky Blue venetian blind pattern.
- **Bottom-Right (Diamond Cutout)**: Fragments within Manhattan distance $|u - 0.5| + |v - 0.5| < 0.32$ are discarded, forming a centered diamond aperture in a Golden Yellow card.

In all four quadrants, discarded fragments must not write to the color attachment, allowing the clear background to show through cleanly. If fragment `discard` is unsupported or improperly handled, the quads will render solid without cutouts.

## Reference
![Reference](reference.png "Reference")
