#include "Sprite.hlsli"

// C++のTransformationMatrixと同じレイアウト。照明用の法線は扱わない。
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct SpriteVertexInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

SpriteVertexOutput main(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    return output;
}
