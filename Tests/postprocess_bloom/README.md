# Post-Process Bloom Visual Test

Validates the dual-filtering threshold Bloom pass (`BloomPass`):
- 5-level downsampling pyramid with Karis anti-flicker 13-tap filter
- Soft-knee quadratic threshold extraction
- 5-level upsampling pyramid with 9-tap tent filter
- Additive blending of glowing halos around high-dynamic-range (> 1.0) emissive emblems
