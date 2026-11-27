#include "Particle.hlsli"

struct Material
{
    float4 color;
    int32_t lightingMode;
    float3 padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    float4 texel = gTexture.Sample(gSampler, uv);
    clip(texel.a - 0.5f);

    // 元の緑色の葉の明暗と葉脈を残し、粒ごとの秋色で描く。
    float pattern = 0.35f + 0.65f * texel.g;
    float3 lightDirection = normalize(float3(0.35f, 0.8f, -0.45f));
    float brightness = 0.55f + 0.45f * abs(dot(normalize(input.normal), lightDirection));
    return float4(gMaterial.color.rgb * input.color.rgb * pattern * brightness, 1.0f);
}
