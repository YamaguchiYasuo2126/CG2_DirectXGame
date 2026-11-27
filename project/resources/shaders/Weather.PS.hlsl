struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PSInput input) : SV_TARGET0
{
    // 雨筋の上下を薄くして、四角い輪郭を目立たなくする。
    float fade = saturate(1.0f - abs(input.uv.y * 2.0f - 1.0f));
    return float4(input.color.rgb, input.color.a * fade);
}
