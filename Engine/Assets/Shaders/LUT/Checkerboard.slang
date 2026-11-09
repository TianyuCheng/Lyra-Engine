import lyra;

[[lyra::binding(0, 0)]]
RWTexture2D<float4> out_checkerboard;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 pixel = dispatch_thread_id.xy;
    if (pixel.x >= 16 || pixel.y >= 16) {
        return;
    }

    uint tile_size = 4;
    bool is_even = ((pixel.x / tile_size) + (pixel.y / tile_size)) % 2 == 0;

    float4 color_dark  = float4(50.0 / 255.0, 50.0 / 255.0, 50.0 / 255.0, 1.0);
    float4 color_light = float4(70.0 / 255.0, 70.0 / 255.0, 70.0 / 255.0, 1.0);

    out_checkerboard[pixel] = is_even ? color_dark : color_light;
}
