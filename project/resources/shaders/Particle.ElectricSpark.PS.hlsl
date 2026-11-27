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
    float core = exp(-30.0f * p.x * p.x - 3.0f * p.y * p.y);
    float halo = exp(-3.5f * dot(p, p));
    float strength = saturate(core + 0.45f * halo) * input.color.a;
    float3 color = lerp(input.color.rgb, float3(1.0f, 1.0f, 1.0f), core * 0.9f);
    return float4(color * gMaterial.color.rgb, strength);
}
