#include "Particle.hlsli"

struct Material
{
    float4 color;
    int32_t lightingMode;
    // C++側の float padding[3]; に対応させるための隙間
    float3 padding;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0); // (bはConstantBufferを意味する)
Texture2D<float4> gTexture : register(t0); // (tはSRVのregisterを意味する)
SamplerState gSampler : register(s0); // (sはSamplerのregisterを意味する)

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (textureColor.a <= 0.5f)
    {
        discard;
    }

    const float3 lightDirection = normalize(float3(0.35f, 0.8f, -0.45f));
    const float diffuse = saturate(dot(normalize(input.normal), lightDirection));
    const float brightness = gMaterial.lightingMode == 0 ? 1.0f : 0.3f + 0.7f * diffuse;
    output.color = float4(gMaterial.color.rgb * textureColor.rgb * input.color.rgb * brightness, 1.0f);

    return output;
}
