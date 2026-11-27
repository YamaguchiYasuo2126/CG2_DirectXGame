struct RainInstance
{
    float4x4 WVP;
    float4 color;
};

StructuredBuffer<RainInstance> gRain : register(t0);

struct VSInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;
    output.position = mul(input.position, gRain[instanceId].WVP);
    output.uv = input.uv;
    output.color = gRain[instanceId].color;
    return output;
}
