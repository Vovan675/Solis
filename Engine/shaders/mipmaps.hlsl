Texture2D texture;

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;
    output.color = float4(1, 0, 0, 1);
    return output;
}