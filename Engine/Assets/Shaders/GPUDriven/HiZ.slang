import lyra;

struct HiZParams
{
    uint2 src_resolution;
    uint2 dst_resolution;
    uint  is_reverse_z;
    uint  padding[3];
};

[[lyra::immediates]]
ConstantBuffer<HiZParams> params;

[[lyra::binding(0, 0)]]
Texture2D<float> input_depth;

[[lyra::binding(1, 0)]]
RWTexture2D<float> output_depth;

[shader("compute")]
[numthreads(8, 8, 1)]
void csmain(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 coord = dispatch_thread_id.xy;
    if (coord.x >= params.dst_resolution.x || coord.y >= params.dst_resolution.y) {
        return;
    }

    uint2 src_coord = coord * 2;
    uint2 max_src   = params.src_resolution - 1;

    uint2 c0 = min(src_coord + uint2(0, 0), max_src);
    uint2 c1 = min(src_coord + uint2(1, 0), max_src);
    uint2 c2 = min(src_coord + uint2(0, 1), max_src);
    uint2 c3 = min(src_coord + uint2(1, 1), max_src);

    float d0 = input_depth[c0];
    float d1 = input_depth[c1];
    float d2 = input_depth[c2];
    float d3 = input_depth[c3];

    float result;
    if (params.is_reverse_z != 0) {
        result = min(min(d0, d1), min(d2, d3));
    } else {
        result = max(max(d0, d1), max(d2, d3));
    }

    output_depth[coord] = result;
}
