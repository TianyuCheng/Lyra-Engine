# Milestone 2: Hardware Path Tracer & Advanced Resampling (ReSTIR)

**Goal**: Complete the reference path tracer and real-time reservoir resampling pipeline in `Modules/Rendering/PathTracer`, supporting hardware DXR/Vulkan ray tracing with fallback to SoftGPU Embree, ReSTIR DI/GI/PT, and world-space radiance caching, verified by progressive and real-time image testing.

[← Return to Master Roadmap](file:///Users/tcheng/Repositories/Lyra-Engine/Roadmap/README.md)

---

## 2.2 Monte Carlo Multi-Bounce Global Illumination
- [ ] **2.2.3 Stochastic Volumetric Participating Media & Delta Tracking**
  - Shaders: [`Engine/Assets/Shaders/PathTracer/PathTracer.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/PathTracer/PathTracer.slang), [`Engine/Assets/Shaders/Common/RayTracing.slang`](file:///Users/tcheng/Repositories/Lyra-Engine/Engine/Assets/Shaders/Common/RayTracing.slang)
  - *Current Status*: Henyey-Greenstein anisotropic phase function importance sampling (`sample_henyey_greenstein`), phase evaluation (`henyey_greenstein_phase`), and Beer-Lambert attenuation are implemented in `Common/RayTracing.slang`. Full null-collision delta tracking random walk in the primary path tracing kernel remains pending.
  - **Null-Collision Free-Flight Sampling (Woodcock / Delta Tracking)**:
    - Homogenize heterogeneous medium by introducing a fictitious null-collision scattering cross-section $\sigma_n(x) = \bar{\sigma}_t - \sigma_t(x) \ge 0$ bounded by majorant extinction $\bar{\sigma}_t \ge \max_{x} \sigma_t(x)$.
    - Free-flight distance step recurrence:
      $$t \leftarrow t - \frac{\ln(1 - \xi)}{\bar{\sigma}_t}$$
    - Russian roulette collision classification: real interaction with probability $P_{\text{real}} = \sigma_t(x) / \bar{\sigma}_t$; null collision (continue straight with unchanged throughput) with probability $P_{\text{null}} = 1 - P_{\text{real}}$.
    - Single-scattering albedo selection $\alpha_s = \sigma_s(x) / \sigma_t(x)$ determining absorption vs. scattering event.
  - **Henyey-Greenstein Phase Function Importance Sampling**:
    - Directional sampling via inverse CDF of Henyey-Greenstein phase function:
      $$\cos\theta = \begin{cases} 1 - 2\xi_1, & \text{if } |g| < 10^{-3} \\ \frac{1}{2g}\left(1 + g^2 - \left(\frac{1 - g^2}{1 - g + 2g\xi_1}\right)^2\right), & \text{otherwise} \end{cases}, \quad \phi = 2\pi\xi_2$$
  - **Volumetric Next-Event Estimation (NEE) & Ratio Tracking**:
    - Trace shadow rays from medium scatter points towards analytical lights; evaluate beam transmittance using unweighted Ratio Tracking to prevent high variance from multi-sample marching.
  - Serves as the ground truth reference baseline for validating real-time froxel volumetric fog and cloudscape approximation passes.

---

## 2.5 Reservoir-Based Spatio-Temporal Importance Resampling (ReSTIR DI / GI / PT)
- [ ] **2.5.1 ReSTIR DI (Direct Illumination with Many-Light Reservoir Sampling)**
  - Files: `Engine/Source/Rendering/Passes/ReSTIRDIPass.cpp`, `Engine/Include/Lyra/Rendering/Passes/ReSTIRDIPass.h`
  - Shaders: `Engine/Assets/Shaders/PathTracer/ReSTIR_DI.slang`, `Engine/Assets/Shaders/Common/Reservoir.slang`
  - **Core Reservoir Structure (`ReservoirDI`)**:
    ```slang
    struct ReservoirDI
    {
        uint  selected_light_index; // 4 bytes: index of chosen light / emitter candidate
        float weight_sum;           // 4 bytes: sum of candidate weights w_i
        float M;                    // 4 bytes: effective sample count / history length
        float W;                    // 4 bytes: unbiased contribution weight = (1 / p_hat) * (weight_sum / M)
    }; // 16 bytes aligned
    ```
  - **Candidate Generation (Initial RIS)**:
    - Generate $M_{\text{init}} = 32$ candidates per pixel by uniform or power-scaled CDF sampling over dynamic analytical lights and emissive mesh triangles.
    - Evaluate unshadowed target distribution $\hat{p}(y) = \|\text{BSDF}(\vec{x}, \vec{\omega}_o, \vec{\omega}_i) \odot L_e(y) \cos\theta_i\|$.
    - Accumulate into reservoir: $w_i = \hat{p}(y_i) / p(y_i)$; update chosen candidate with probability $w_i / \sum w$.
  - **Temporal Resampling Pass**:
    - Reproject pixel position $\vec{x}_N$ to previous frame $N-1$ using camera velocity vectors.
    - Validate geometry similarity: $|\vec{n}_N \cdot \vec{n}_{N-1}| \ge 0.9$ and $|z_N - z_{N-1}| / z_N \le 0.05$.
    - Clamp temporal history length $M_{\text{prev}} \le M_{\max} \approx 20-30$ to prevent history lag and ghosting on dynamic lighting changes.
    - Merge temporal reservoir: $W_{\text{temporal}} = \frac{1}{\hat{p}(z)} \frac{w_1 + w_2}{M_1 + M_2}$.
  - **Spatial Resampling & Pairwise Visibility Reuse**:
    - Disperse $K \in [3, 5]$ spatial sample taps within radius $R \approx 16-32$ pixels.
    - Pairwise visibility / unbiased MIS weighting: evaluate candidate visibility $\vec{x} \leftrightarrow y$ via single shadow ray; discard occluded reservoirs without introducing bias.
- [ ] **2.5.2 ReSTIR GI (Indirect Illumination & Screen-Space Reservoir Resampling)**
  - Files: `Engine/Source/Rendering/Passes/ReSTIRGIPass.cpp`, `Engine/Include/Lyra/Rendering/Passes/ReSTIRGIPass.h`
  - Shaders: `Engine/Assets/Shaders/PathTracer/ReSTIR_GI.slang`, `Engine/Assets/Shaders/Common/ReservoirGI.slang`
  - **Core Reservoir Structure (`ReservoirGI`)**:
    ```slang
    struct ReservoirGI
    {
        float3 sample_point;     // 12 bytes: secondary hit position x_s in world space
        float  weight_sum;       // 4 bytes: accumulated weights
        float3 sample_normal;    // 12 bytes: normal at secondary hit point n_s
        float  M;                // 4 bytes: effective sample count
        float3 radiance;         // 12 bytes: incident radiance Li(x_s -> x)
        float  W;                // 4 bytes: unbiased contribution weight
        uint   sample_material;  // 4 bytes: material ID / flags at secondary hit point
        uint3  padding;          // 12 bytes: 64-byte total alignment
    };
    ```
  - **Initial Candidate Ray Generation**:
    - Trace 1 indirect path ray per pixel via BSDF VNDF sampling; record secondary hit position $\vec{x}_s$, surface normal $\vec{n}_s$, and direct radiance $L_o(\vec{x}_s)$.
    - Target distribution: $\hat{p}(\vec{x}_s) = \|\text{BSDF}(\vec{x}, \vec{\omega}_o, \vec{\omega}_i) \odot L_i(\vec{x}_s) \cos\theta_i\|$.
  - **Jacobian Geometry Shift Mapping**:
    - When transferring secondary sample $\vec{x}_s$ from neighbor $\vec{x}_q$ to center $\vec{x}_p$, compute Jacobian determinant of the change in solid angle:
      $$J_{q \to p} = \frac{\cos\theta'_s}{\cos\theta_s} \frac{\|\vec{x}_s - \vec{x}_q\|^2}{\|\vec{x}_s - \vec{x}_p\|^2}$$
    - Rescale candidate weight: $w' = \hat{p}_p(\vec{x}_s) \cdot J_{q \to p} \cdot W_q \cdot M_q$. Discard shift if $\cos\theta'_s \le 0$ or backfacing.
  - **Permutation Sampling & Boiling Suppression**:
    - Apply pseudo-random spatial permutation sampling across frames to decorrelate neighborhood reuse patterns and eliminate energy spikes / "firefly boiling" artifacts.
- [ ] **2.5.3 ReSTIR PT (Path Reuse via Shift Mapping & Manifold Exploration)**
  - Shaders: `Engine/Assets/Shaders/PathTracer/ReSTIR_PT.slang`, `Engine/Assets/Shaders/PathTracer/PathShift.slang`
  - Extends ReSTIR GI from 1-bounce indirect hits to arbitrary length multi-bounce light paths ($\bar{x} = x_0 x_1 \dots x_k$).
  - **Shift Mapping Formulations**:
    - **Random Replay Shift**: For glossy-to-specular paths, replay the random number sequence of the primary ray at the shifted pixel, evaluating trajectory convergence.
    - **Reconnection Shift**: For diffuse-to-diffuse vertices along the path, reconnect vertex $x_1$ directly to shifted camera vertex $x'_0$, multiplying by the solid-angle Jacobian:
      $$J = \left|\frac{\mathrm{d}\mu(x_1)}{\mathrm{d}\mu'(x_1)}\right|$$
    - **Half-Vector / Manifold Exploration Shift**: For specular-diffuse-specular (SDS) caustics paths, enforce Snell's law / reflection constraint by solving the half-vector specular manifold equation $\vec{h}(\vec{x}'_1) - \vec{n}(\vec{x}'_1) = 0$, capturing sharp focused caustics with spatial reuse.
- [ ] **2.5.4 World-Space Spatial Hash-Grid Radiance Cache (Multi-Bounce Extension)**
  - Files: `Engine/Source/Rendering/Passes/RadianceCachePass.cpp`, `Engine/Include/Lyra/Rendering/Passes/RadianceCachePass.h`
  - Shaders: `Engine/Assets/Shaders/PathTracer/RadianceCache.slang`, `Engine/Assets/Shaders/Common/SpatialHash.slang`
  - **Multi-Level Morton Hash Grid**:
    - Hierarchical spatial hash table storing compact spherical harmonics (SH2/SH3) or directional oct-encoded irradiance probes in world space.
    - Cache lookup for rays beyond bounce $k \ge 2$, cutting off infinite recursive ray tracing while supplying stable, noise-free incident radiance to ReSTIR GI/PT.
    - Asynchronous probe updates via EMA temporal smoothing:
      $$E_{\text{cache}} \leftarrow (1 - \alpha) E_{\text{cache}} + \alpha E_{\text{new}}$$
- [ ] **2.5.5 ReSTIR Many-Light & Indirect GI Visual Test Fixtures**
  - Directories: `Tests/restir_di/`, `Tests/restir_gi/`
  - **`Tests/restir_di/`**:
    - Test scene: Large architectural hall illuminated by $1,024$ dynamic colored point/spot lights and emissive mesh geometries.
    - Verification: Compare 1-spp ReSTIR DI resolve against a 4096-spp ground-truth reference rendered by the path tracer ($\text{RMSE} \le 1.8$, $\text{mismatch} \le 1.0\%$).
  - **`Tests/restir_gi/`**:
    - Test scene: Multi-room indirect illumination chamber featuring strong colored diffuse bounce and glossy reflections.
    - Verification: Compare interactive 1-spp ReSTIR GI indirect lighting against 2048-spp path tracer baseline ($\text{RMSE} \le 2.0$, $\text{mismatch} \le 1.0\%$).
  - Validate with `just test restir_di` and `just test restir_gi`.
