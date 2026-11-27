struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    // 画面を覆う1枚の三角形。頂点バッファは使わない。
    float2 position = vertexId == 0 ? float2(-1.0f, -1.0f) :
        (vertexId == 1 ? float2(-1.0f, 3.0f) : float2(3.0f, -1.0f));
    VSOutput output;
    output.position = float4(position, 0.0f, 1.0f);
    output.uv = position * float2(0.5f, -0.5f) + 0.5f;
    return output;
}
