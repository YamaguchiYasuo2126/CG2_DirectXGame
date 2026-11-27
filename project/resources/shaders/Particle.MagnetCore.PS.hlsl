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
    float radius = length(p);
    float angle = atan2(p.y, p.x);
    float time = input.color.a;
    float ring = exp(-95.0f * pow(radius - 0.66f, 2.0f));
    float spiral = exp(-80.0f * pow(radius - 0.30f -
        0.13f * sin(angle * 3.0f - time * 5.0f), 2.0f));
    float shimmer = 0.65f + 0.35f * sin(angle * 10.0f + time * 11.0f);
    float centerGlow = exp(-18.0f * radius * radius);
    float strength = saturate(ring * shimmer * 0.75f + spiral * 0.55f +
        centerGlow * 0.70f);
    float3 color = lerp(float3(0.28f, 0.75f, 1.0f),
        float3(0.85f, 0.55f, 1.0f), saturate(spiral + centerGlow));
    return float4(color * input.color.rgb * gMaterial.color.rgb, strength * 0.85f);
}
