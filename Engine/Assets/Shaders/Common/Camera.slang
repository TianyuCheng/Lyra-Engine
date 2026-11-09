module Camera;

/**
 * @brief Core camera uniforms for geometric transforms and projection.
 * Lightweight and cache-efficient for early-Z, shadow, depth, and raster passes.
 */
public struct CameraUniforms
{
    public float4x4 view;
    public float4x4 proj;
    public float4x4 view_proj;
    public float4x4 inv_view;
    public float4x4 inv_proj;
    public float4x4 inv_view_proj;
    public float4x4 prev_view_proj;

    public float3   camera_pos;
    public float    near_plane;

    public float2   jitter_offset;
    public float2   render_extent;

    public float    far_plane;
    public float    fov;
    public float2   padding;
};

/**
 * @brief Physically based camera exposure, eye adaptation, and EV100 buffer.
 * Dedicated separate uniform buffer bound only for post-processing and physical lighting passes.
 */
public struct ExposureUniforms
{
    public float exposure_scale; // physical exposure scale factor: 1.0 / (1.2 * 2^EV100)
    public float ev100;          // current exposure value EV100
    public float pre_exposure;   // pre-exposure multiplier for FP16 dynamic range preservation
    public uint  exposure_mode;  // 0: Manual, 1: EV Compensation, 2: Auto Histogram

    public float min_ev;         // min EV clamp for eye adaptation
    public float max_ev;         // max EV clamp for eye adaptation
    public float speed_up;       // adaptation speed towards brighter environments
    public float speed_down;     // adaptation speed towards darker environments

    public float low_percent;    // discard bottom % darkest pixels
    public float high_percent;   // discard top % brightest pixels
    public float compensation;   // exposure compensation in EV stops
    public float padding;
};

/**
 * @brief Converts EV100 to physical linear exposure scale (Filament calibration constant C = 1.2).
 */
public float ev100_to_exposure(float ev100)
{
    return 1.0 / (1.2 * pow(2.0, ev100));
}

/**
 * @brief Computes EV100 from physical camera aperture (f-stop N), shutter speed (t in seconds), and ISO sensitivity (S).
 */
public float camera_parameters_to_ev100(float aperture, float shutter_speed, float iso, float compensation = 0.0)
{
    float ev = log2((aperture * aperture / max(shutter_speed, 1e-5)) * (100.0 / max(iso, 1e-5)));
    return ev - compensation;
}
