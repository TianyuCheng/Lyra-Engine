# Volumetric Clouds Test Fixture

Visual regression test for physical Real-Time Volumetric Cloudscapes (Milestone 3, Task 3.4).

## Test Scenario
- Wide outdoor vista with physical atmospheric scattering skybox (Hillaire 2020 / Bruneton 2008 3-LUT model).
- Directional sun at low elevation angle ($15^\circ$, sunset / golden hour).
- Procedural multi-octave cumulus cloudscapes:
  - Low-frequency base Perlin-Worley shape modeling.
  - High-frequency Worley cauliflower edge erosion.
  - Schneider dual-lobe forward-scattering phase function (silver-lining solar halo).
  - Beer-Powder crevasse darkening.
  - Multiple scattering energy transmission boost.
