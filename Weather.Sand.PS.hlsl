struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET0
{
    // 小さな砂粒にも大きな砂煙にも使える、柔らかい円形の濃淡。
    float2 centered = input.uv * 2.0f - 1.0f;
    float radiusSquared = dot(centered, centered);
    float alpha = exp(-2.5f * radiusSquared) * input.color.a;
    return float4(input.color.rgb, alpha);
}
