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
    // 複数の柔らかな円を重ね、四角形の境界が見えない煙にする。
    float2 p = input.texcoord * 2.0f - 1.0f;
    float mainPuff = exp(-3.8f * dot(p, p));
    float2 offsetA = p - float2(-0.22f, 0.14f);
    float2 offsetB = p - float2(0.22f, -0.12f);
    float density = mainPuff + 0.30f * exp(-8.0f * dot(offsetA, offsetA)) +
        0.30f * exp(-8.0f * dot(offsetB, offsetB));
    float edge = 1.0f - smoothstep(0.70f, 1.0f, length(p));
    float alpha = saturate(density * edge) * input.color.a * gMaterial.color.a;
    return float4(input.color.rgb * gMaterial.color.rgb, alpha);
}
