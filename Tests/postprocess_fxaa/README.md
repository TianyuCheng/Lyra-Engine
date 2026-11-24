# Post-Process FXAA Visual Test

Validates the Fast Approximate Anti-Aliasing pass (`FXAAPass`):
- High-contrast edge detection using Rec.709 perceived luminance
- Edge orientation search (horizontal vs vertical) and end-of-edge traversal
- Sub-pixel anti-aliasing blending across diagonal geometric edges
