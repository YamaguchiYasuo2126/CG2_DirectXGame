#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int32_t enableLighting;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<Material> gMaterial : register(b0); // (bはConstantBufferを意味する)

// 平行光源用のConstantBuffer
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

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
    
    if (gMaterial.enableLighting != 0)
    {
        // Half Lambert
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // 最終的な色 = マテリアル色 × テクスチャ色 × ライト色 × cosθ × 輝度
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        // Lightingしない場合 (前回までと同じ演算)
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}