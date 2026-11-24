# Atmosphere & Image-Based Lighting (IBL) Visual Regression Test

Verifies physically based atmospheric scattering with multiple scattering, Rayleigh and Mie scattering, Chappuis ozone absorption, and solar disc with limb darkening combined with Split-Sum Image-Based Lighting (IBL).

## Test Scene
- Physically based skybox and directional sun light.
- 4x4 matrix of spheres demonstrating:
  - Columns: varying roughness factors [0.05, 0.35, 0.65, 0.95]
  - Rows: varying metallic factors [0.0, 0.33, 0.67, 1.0]
- Prefiltered HDR environment cubemap with convolved specular reflection mip chain and 9-coefficient Spherical Harmonics (SH9) diffuse irradiance.
- Floating sphere array providing unobstructed 360° reflection and irradiance of both upper atmosphere and lower horizon environment.
