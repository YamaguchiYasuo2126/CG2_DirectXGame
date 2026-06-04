#include "Object3d.hlsli"

struct Material {
    float4 color;
};

ConstantBuffer<Material> gMaterial : register(b0); // (bはConstantBufferを意味する)

Texture2D<float4> gTexture : register(t0); // (tはSRVのregisterを意味する)
SamplerState gSampler : register(s0); // (sはSamplerのregisterを意味する)

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    // UV座標のXが 1.0 より大きければ、テクスチャを適用せずマテリアルの色（C++で計算した色）のみにする
    if (input.texcoord.x > 1.0f)
    {
        output.color = gMaterial.color;
    }
    else
    {
        // 通常の三角形は今まで通りテクスチャを適用
        float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
        output.color = gMaterial.color * textureColor;
    }
    return output;
}