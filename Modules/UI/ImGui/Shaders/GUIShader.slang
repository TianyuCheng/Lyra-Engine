import lyra;

struct VertexInput
{
    float2 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct VertexOutput
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct TextureInput
{
    Texture2D<float4> tex;
    SamplerState      smp;
};

[[vk::push_constant]]
ConstantBuffer<float4x4> xform : PUSH_CONSTANT;

ParameterBlock<TextureInput> image;

[shader("vertex")]
VertexOutput vsmain(VertexInput input)
{
    VertexOutput output;
    output.pos   = mul(float4(input.pos, 0.0, 1.0), xform);
    output.uv    = input.uv;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fsmain(VertexOutput input) : SV_Target
{
    return image.tex.Sample(image.smp, input.uv) * input.color;
}
