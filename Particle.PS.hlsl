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
    
    output.color = gMaterial.color * textureColor;

    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}
