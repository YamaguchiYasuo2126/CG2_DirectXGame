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
    // 下側を広く、先端を細くした柔らかな炎を四角形の内側に描く。
    float height = saturate(1.0f - input.texcoord.y);
    float horizontal = input.texcoord.x * 2.0f - 1.0f;
    float halfWidth = lerp(0.92f, 0.04f, height);
    float edge = 1.0f - smoothstep(0.45f, 1.0f, abs(horizontal) / halfWidth);
    float vertical = smoothstep(0.0f, 0.18f, height) *
        (1.0f - smoothstep(0.75f, 1.0f, height));
    float intensity = edge * vertical;
    float3 core = lerp(float3(1.0f, 0.42f, 0.06f),
        float3(1.0f, 0.92f, 0.52f), saturate(1.0f - height));
    return float4(core * input.color.rgb * gMaterial.color.rgb,
        intensity * input.color.a * gMaterial.color.a * 0.65f);
}
