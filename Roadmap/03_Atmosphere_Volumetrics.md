# Milestone 3: Environment, Atmospheres, Volumetrics & Post-Processing Stack

**Goal**: Complete the unified froxel-based volumetric fog pipeline, clustered participating media in-scattering, 3D noise texture cooking, and directional cloud shadows, accompanied by automated visual regression fixtures.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 3.3 Unified Froxel-Based Volumetric Fog & Lighting
- [ ] **3.3.1 3D Froxel Grid Architecture & Density Injection**
  - Files: `Engine/Source/Rendering/Passes/VolumetricFogPass.cpp`, `Engine/Include/Lyra/Rendering/Passes/VolumetricFogPass.h`
  - Shaders: `Engine/Assets/Shaders/Volumetrics/VolumetricCommon.slang`, `Engine/Assets/Shaders/Volumetrics/VolumetricDensityInject.slang`
  - **ECS Component Contracts**:
    - `VolumetricFogComponent` ([`Engine/Include/Lyra/Components/VolumetricFog.h`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Include/Lyra/Components/VolumetricFog.h)):
      ```cpp
      struct [[lyra::component("Volumetric Fog", category = "Lighting", icon = "LYRA_ICON_FOG")]] VolumetricFog
      {
          Vector3 scattering_albedo = Vector3(0.9f);       // sigma_s color tint (12 bytes)
          float   density_scale     = 0.02f;               // rho_0 global base density (4 bytes)
          float   height_falloff    = 0.1f;                // lambda exponential decay (4 bytes)
          float   height_offset     = 0.0f;                // y_0 reference height (4 bytes)
          float   anisotropy_g      = 0.65f;               // forward Mie asymmetry in [-1, 1] (4 bytes)
          float   max_distance      = 150.0f;              // max volumetric depth in meters (4 bytes)
          Vector3 ambient_tint      = Vector3(0.05f);      // ambient in-scatter floor (12 bytes)
          float   extinction_factor = 1.0f;                // extinction multiplier (4 bytes)
      };
      ```
    - `LocalFogVolumeComponent`:
      ```cpp
      enum class FogVolumeShape : uint32_t { BOX = 0, SPHERE = 1, ELLIPSOID = 2, };
      struct [[lyra::component("Local Fog Volume", category = "Lighting", icon = "LYRA_ICON_VOLUME")]] LocalFogVolume
      {
          FogVolumeShape shape         = FogVolumeShape::BOX;
          Vector3        half_extents  = Vector3(5.0f);
          Vector3        color         = Vector3(1.0f);
          float          density       = 0.1f;
          float          edge_softness = 0.3f;
          float          noise_scale   = 0.5f;
          float          noise_speed   = 0.2f;
      };
      ```
  - **Froxel Discretization & Exponential Depth Mapping**:
    - Frustum-aligned 3D voxel grid ($160 \times 90 \times 64$, matching clustered shading aspect ratio).
    - Map depth slice index $w \in [0, N_z-1]$ to view-space depth $z$:
      $$z(w) = z_{\text{near}} \cdot \left(\frac{z_{\text{far}}}{z_{\text{near}}}\right)^{\frac{w + 0.5}{N_z}}, \quad w(z) = \left\lfloor N_z \frac{\ln(z / z_{\text{near}})}{\ln(z_{\text{far}} / z_{\text{near}})} \right\rfloor$$
    - Compute pass injecting global exponential height fog:
      $$\rho(y) = \rho_0 \cdot \exp\left(-\lambda (y - y_0)\right)$$
    - Local volume injection: evaluate box AABB / sphere distance with smooth edge feathering factor:
      $$\alpha_{\text{edge}} = \text{saturate}\left(\frac{1 - d_{\text{norm}}}{\text{edge\_softness}}\right)$$
    - Output Target: `FroxelDensityBuffer` (`GPUTextureFormat::RGBA16FLOAT`, $160 \times 90 \times 64$, storing scattering cross-section $\vec{\sigma}_s$ in `RGB` and extinction $\sigma_t$ in `A`).
- [ ] **3.3.2 Clustered Light In-Scattering & Shadow Evaluation**
  - Shader: `Engine/Assets/Shaders/Volumetrics/VolumetricLightInject.slang`
  - Compute threadgroup `[numthreads(8, 8, 1)]` evaluating participating media in-scattering per froxel cell:
    - **Directional Sunlight with CSM Shadows**:
      - Transform froxel center to light view-space; select cascade split index $i \in [0, 3]$.
      - Sample Cascaded Shadow Map (CSM) using $2 \times 2$ PCF with normal/slope bias to eliminate shadow acne.
      - Evaluate Henyey-Greenstein phase function for solar ray angle $\theta = \arccos(\vec{\omega}_i \cdot \vec{\omega}_v)$:
        $$\mathcal{P}_{\text{HG}}(\theta, g) = \frac{1 - g^2}{4\pi (1 + g^2 - 2g\cos\theta)^{3/2}}$$
    - **Clustered Analytical Local Lights**:
      - Read 3D cluster tile `(offset, count)` from `LightGridSSBO` and light indices from `LightIndexListSSBO`.
      - Loop over active Point and Spot lights; evaluate inverse-square attenuation with smooth windowed falloff $f_{\text{att}} = \left(\text{saturate}\left(1 - (d/r)^4\right)\right)^2 / (d^2 + 1)$.
      - Evaluate local light forward scattering phase function $\mathcal{P}(\theta_l, g)$.
  - Output Target: `FroxelScatteringBuffer` (`GPUTextureFormat::RGBA16FLOAT`, $160 \times 90 \times 64$, storing in-scattered radiance $L_{\text{scat}}$ in `RGB` and extinction $\sigma_t$ in `A`).
- [ ] **3.3.3 Temporal Reprojection & Volume Integration**
  - Shaders: `Engine/Assets/Shaders/Volumetrics/VolumetricTemporalFilter.slang`, `Engine/Assets/Shaders/Volumetrics/VolumetricIntegration.slang`
  - **Temporal Filter Pass**:
    - Reproject froxel center from frame $N-1$ using previous frame camera inverse view-projection matrix and velocity vectors.
    - Sub-froxel spatial jittering via 3D blue-noise sequence to break up wood-grain slicing artifacts.
    - Exponential Moving Average (EMA) blend factor $\alpha = 0.95$ with $3 \times 3 \times 3$ neighborhood bounding box clamping in YCoCg color space to eliminate ghosting during camera motion.
  - **Prefix-Sum Integration Pass (Front-to-Back Raymarch)**:
    - Compute dispatch `[numthreads(8, 8, 1)]` marching front-to-back along depth slice $k \in [0, N_z-1]$:
      $$\Delta z_k = z_{\text{slice}}(k + 1) - z_{\text{slice}}(k)$$
      $$T_{\text{step}}(k) = \exp\left(-\sigma_t(k) \cdot \Delta z_k\right)$$
      $$L_{\text{slice}}(k) = L_{\text{scat}}(k) \cdot \left(\frac{1 - T_{\text{step}}(k)}{\sigma_t(k)}\right)$$
      $$L_{\text{accum}}(k) = L_{\text{accum}}(k-1) + T_{\text{accum}}(k-1) \cdot L_{\text{slice}}(k)$$
      $$T_{\text{accum}}(k) = T_{\text{accum}}(k-1) \cdot T_{\text{step}}(k)$$
    - Output Target: `IntegratedFroxelVolume` (`GPUTextureFormat::RGBA16FLOAT`, $160 \times 90 \times 64$, storing accumulated scattered radiance in `RGB` and transmittance in `A`).
- [ ] **3.3.4 Volumetric Fog Scene Compositing Pass**
  - Shader: `Engine/Assets/Shaders/Volumetrics/VolumetricComposite.slang` (and deferred lighting composite integration)
  - Fullscreen raster or compute pass sampling `IntegratedFroxelVolume` with trilinear filtering using pixel depth $z$:
    $$C_{\text{final}} = C_{\text{scene}} \cdot T(u, v, z) + L_{\text{accum}}(u, v, z)$$
  - **Bilateral Depth-Aware Upsampling**:
    - Sample $2 \times 2$ nearest froxels; compute depth cross-bilateral weights:
      $$w_i = \exp\left(-\frac{|z_{\text{pixel}} - z_{\text{froxel}, i}|}{\sigma_z \cdot |\nabla z| + \epsilon}\right)$$
    - Prevents volumetric fog bleeding and halo artifacts along foreground geometry silhouettes.
- [ ] **3.3.5 Volumetric Fog Visual Regression Test Fixture**
  - Directory: `Tests/volumetric_fog/`
  - Scene Setup: Interior colonnade hall with high arched windows; directional sunlight casting distinct crepuscular ray shafts through window openings, intersected by colored local point lights immersed in exponential ground fog.
  - Image Verification: Compare rendered output against golden `reference.png` via [`runtest.py`](file:///Users/tcheng/Repositories/Lyra-Engine/Tools/Scripts/runtest.py) ($\text{RMSE} \le 1.5$, $\text{mismatch} \le 0.5\%$).
  - Validate with `just test volumetric_fog`.

---

## 3.4 Real-Time Volumetric Cloudscapes
- [ ] **3.4.1 3D Noise Asset Generation & Baking (Perlin-Worley & Worley 3D Textures)**
  - Module / Shader: `Modules/Assets/Texture/NoiseCookerPlugin.cpp`, `Engine/Assets/Shaders/Noise/NoiseBake.slang`
  - **Cooker Tasks & Asset Chunks**:
    - Cook tileable 3D Perlin-Worley base shape noise texture ($128 \times 128 \times 128$, `GPUTextureFormat::RGBA8UNORM` or `BC4`/`BC5` compressed):
      - `R`: Perlin-Worley hybrid base noise (Perlin noise modulated by low-frequency Worley noise).
      - `G`: Worley cellular noise octave 1 ($f = 2$).
      - `B`: Worley cellular noise octave 2 ($f = 4$).
      - `A`: Worley cellular noise octave 3 ($f = 8$).
    - Cook tileable 3D Worley high-frequency detail erosion noise texture ($32 \times 32 \times 32$, `GPUTextureFormat::RGB8UNORM`):
      - 3 octaves of fine Worley cellular noise for eroding billowing cloud boundaries.
    - Cook 2D Weather Map ($1024 \times 1024$, `GPUTextureFormat::RGBA8UNORM`):
      - `R`: Cloud coverage fraction ($[0, 1]$).
      - `G`: Precipitation / density scale.
      - `B`: Cloud type gradient ($0.0 = \text{Stratus}$, $0.5 = \text{Stratocumulus}$, $1.0 = \text{Cumulus}$).
      - `A`: Cloud ceiling altitude offset.
    - Cook 2D Curl Noise Map ($256 \times 256$, `GPUTextureFormat::RG8SNORM`) for horizontal turbulent wind perturbation.
    - Store baked textures in `.texture3d` / `.texture` binary formats registered with AMS dependencies.
- [ ] **3.4.3 Aerial Perspective Compositing & Directional Cloud Shadows**
  - Shaders: [`Engine/Assets/Shaders/Sky/Clouds.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/Sky/Clouds.slang), `Engine/Assets/Shaders/Sky/CloudShadow.slang`
  - **Aerial Perspective Integration**:
    - Sample precomputed Bruneton atmospheric transmittance and in-scattering LUTs over the distance $d_{\text{cloud}}$ to cloud surface (`composite_clouds`).
    - Blend accumulated cloud radiance with atmospheric haze and clear sky background.
  - **Directional Cloud Shadow Map Pass**:
    - Render top-down orthographic cloud transmittance map ($512 \times 512$, `GPUTextureFormat::R16FLOAT`) covering a $10 \times 10\,\text{km}$ planar footprint centered around the camera.
    - March downward through the cloud shell along the solar beam vector; evaluate accumulated optical depth $\tau$ and transmittance $T_{\text{sun}} = \exp(-\tau)$.
    - Bind cloud shadow texture in Deferred Lighting resolve, Ocean water pass, and Terrain splatting pass with $2 \times 2$ PCF filtering, casting dynamic cloud shadows across terrain and water.

---

## 3.5 Camera & Object Motion Vectors + Temporal Anti-Aliasing (TAA / FSR 2)
- [ ] **3.5.1 Motion Vector Velocity Pass & Sub-Pixel Camera Jitter**
  - Files: `Engine/Source/Rendering/Passes/VelocityPass.cpp`, `Engine/Include/Lyra/Rendering/Passes/VelocityPass.h`
  - Shaders: `Engine/Assets/Shaders/PostFX/Velocity.slang`, `Engine/Assets/Shaders/Common/CameraJitter.slang`
  - **Sub-Pixel Camera Projection Jitter**:
    - Halton $(2, 3)$ low-discrepancy sequence over an 8- or 16-frame cycle, offsetting projection matrix by $(\delta_x / W, \delta_y / H)$ with $\delta \in [-0.5, +0.5]$.
    - Unjittered projection matrix maintained for shadow maps, motion vector calculation, and depth-based culling.
  - **Screen-Space Velocity Buffer**:
    - Output Target: `VelocityBuffer` (`GPUTextureFormat::RG16FLOAT`, screen resolution), encoding screen-space displacement $(\Delta u, \Delta v) = \mathbf{uv}_{\text{prev}} - \mathbf{uv}_{\text{curr}}$ in UV coordinates $[-1, 1]$.
    - Static geometry: reconstructed in compute resolve pass from depth buffer and $\mathbf{M}_{\text{prev\_vp}} \cdot \mathbf{M}_{\text{curr\_inv\_vp}}$.
    - Dynamic rigid and skinned mesh geometry: dual-transform evaluation in vertex/mesh shader using previous frame transform matrix $\mathbf{M}_{\text{prev\_model}}$ and previous bone palette buffer.
- [ ] **3.5.2 Temporal Anti-Aliasing (TAA) Reconstruction Pass**
  - Files: `Engine/Source/Rendering/Passes/TAAPass.cpp`, `Engine/Include/Lyra/Rendering/Passes/TAAPass.h`
  - Shader: `Engine/Assets/Shaders/PostFX/TAA.slang`
  - **Reprojection & History Sampling**:
    - Fetch screen-space velocity vector $(\Delta u, \Delta v)$ using closest $3 \times 3$ depth dilation ("nearest-depth" tap to avoid background edge bleeding on foreground silhouettes).
    - Sample previous frame history color buffer using bicubic 5-tap Catmull-Rom filtering with negative lobe clipping to eliminate post-filter blurriness.
  - **Color Bounding Box Variance Clipping**:
    - Transform neighborhood color samples into $YCoCg$ color space.
    - Compute statistical mean $\mu$ and variance $\sigma$ of the $3 \times 3$ pixel footprint.
    - Clip or clamp reprojected history color to the bounding box $[\mu - \gamma\sigma, \mu + \gamma\sigma]$ ($\gamma \in [1.0, 1.5]$), eliminating temporal ghosting and disocclusion smearing.
  - **Karis Luminance Weighting & Temporal Blend**:
    - Weight current sample and history sample by inverse perceptual luminance $w = 1 / (1 + L)$ to mitigate specular highlight temporal fireflies.
    - Dynamic exponential blend factor $\alpha \in [0.05, 0.10]$ modulated by velocity magnitude and history clipping distance.
- [ ] **3.5.3 AMD FidelityFX Super Resolution 2 (FSR 2) Integration**
  - Files: `Engine/Source/Rendering/Passes/FSR2Pass.cpp`, `Engine/Include/Lyra/Rendering/Passes/FSR2Pass.h`
  - Native integration with AMD FidelityFX SDK 2.x API.
  - Pass jittered HDR color, native-resolution linear depth, screen-space motion vectors, exposure, and optional reactive masks.
  - Expose quality scaling presets in `RendererConfig` (Quality: $1.5\times$, Balanced: $1.7\times$, Performance: $2.0\times$, Ultra Performance: $3.0\times$).
- [ ] **3.5.4 Temporal Stability & Ghosting Visual Test Fixture**
  - Directory: `Tests/taa_stability/`
  - Scene Setup: High-contrast thin wireframe lattice structure and high-frequency specular normal map spheres rotating across a high-speed camera sweep.
  - Verification: Automated perceptual check verifying absence of temporal fireflies, sub-pixel aliasing crawl, and silhouette ghosting ($\text{RMSE} \le 1.8$, $\text{mismatch} \le 0.5\%$). Validate with `just test taa_stability`.
