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
    float progress = 1.0f - input.color.a;
    float ringRadius = lerp(0.12f, 0.88f, progress);
    float ring = exp(-120.0f * pow(radius - ringRadius, 2.0f));
    float core = exp(-13.0f * radius * radius);
    float rays = pow(abs(sin(atan2(p.y, p.x) * 6.0f)), 16.0f) *
        (1.0f - smoothstep(0.15f, 0.95f, radius));
    float strength = saturate(ring * 0.90f + core * 0.75f + rays * 0.28f) * input.color.a;
    float3 color = lerp(input.color.rgb, float3(1.0f, 1.0f, 1.0f), core);
    return float4(color * gMaterial.color.rgb, strength);
}
