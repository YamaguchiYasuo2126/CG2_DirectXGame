struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET0
{
    // 四角形の中心だけを残し、縁をぼかした雪粒にする。
    float2 centered = input.uv * 2.0f - 1.0f;
    float radiusSquared = dot(centered, centered);
    float alpha = saturate((1.0f - radiusSquared) * 3.0f);
    return float4(input.color.rgb, input.color.a * alpha);
}
