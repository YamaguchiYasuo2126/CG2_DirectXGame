#include "Sprite.hlsli"

// 既存のC++ Materialに合わせた配置。lightingModeは2Dでは使用しない。
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

float4 main(SpriteVertexOutput input) : SV_TARGET0
{
    float2 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform).xy;
    return gMaterial.color * gTexture.Sample(gSampler, uv);
}
