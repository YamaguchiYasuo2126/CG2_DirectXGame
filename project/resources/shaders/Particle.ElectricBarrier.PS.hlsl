#include "Particle.hlsli"

struct Material
{
    float4 color;
    int32_t lightingMode;
    float3 padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float2 p = input.texcoord * 2.0f - 1.0f;
    float time = input.color.a;

    // 両端の電極と、その間を走る放電の筋を描く。
    float railX = exp(-120.0f * pow(abs(p.x) - 0.94f, 2.0f));
    float railY = exp(-120.0f * pow(abs(p.y) - 0.94f, 2.0f));
    float arcA = 0.30f * sin(p.y * 7.0f + time * 10.0f) +
        0.09f * sin(p.y * 23.0f - time * 15.0f);
    float arcB = -0.42f + 0.18f * sin(p.y * 9.0f - time * 13.0f);
    float arcC = 0.47f + 0.13f * sin(p.y * 13.0f + time * 17.0f);
    float strands = exp(-170.0f * pow(p.x - arcA, 2.0f)) +
        0.75f * exp(-180.0f * pow(p.x - arcB, 2.0f)) +
        0.75f * exp(-180.0f * pow(p.x - arcC, 2.0f));
    float flicker = 0.70f + 0.30f * sin(time * 19.0f + p.y * 15.0f);
    float strength = saturate(0.48f * (railX + railY) +
        0.72f * strands * flicker + 0.045f * (1.0f - abs(p.x)));
    float3 color = lerp(input.color.rgb, float3(0.85f, 1.0f, 1.0f),
        saturate(strands * 0.75f));
    return float4(color * gMaterial.color.rgb, strength * 0.80f);
}
