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
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
	output.color = gMaterial.color * textureColor;
    return output;
}