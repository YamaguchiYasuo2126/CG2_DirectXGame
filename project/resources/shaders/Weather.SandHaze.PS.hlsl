struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer SandHaze : register(b0)
{
    float time;
    float opacity;
    float flowX;
    float flowZ;
};

float4 main(PSInput input) : SV_TARGET0
{
    // 風下へ流れる濃淡の帯と細かい砂の揺らぎで、視界全体を霞ませる。
    float2 driftedUV = input.uv - float2(flowX, -flowZ) * time * 0.025f;
    float broadBand = 0.5f + 0.5f * sin(driftedUV.y * 34.0f +
        driftedUV.x * 13.0f);
    float fineBand = 0.5f + 0.5f * sin(driftedUV.y * 135.0f +
        driftedUV.x * 42.0f);
    float2 cell = floor(input.uv * float2(300.0f, 170.0f) -
        float2(flowX, -flowZ) * time * 14.0f);
    float grain = frac(sin(dot(cell, float2(12.9898f, 78.233f))) * 43758.5453f);
    float gust = 0.5f + 0.5f * sin(time * 1.7f);
    float veil = saturate(opacity * (0.55f + 0.18f * broadBand +
        0.11f * fineBand + 0.11f * grain + 0.10f * gust));
    float3 sandColor = lerp(float3(0.67f, 0.53f, 0.35f),
        float3(0.82f, 0.67f, 0.47f), broadBand * 0.7f + grain * 0.15f);
    return float4(sandColor, veil);
}
