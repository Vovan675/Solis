#include "common.h"

// Vertex Shader
struct VSInput {
    float4 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, mul(view, float4(input.position.xyz, 1.0f)));
    output.color = input.color;
    return output;
}

// Pixel Shader
struct PSOutput {
    float4 color : SV_Target;
};

PSOutput PSMain(VSOutput input)
{
    PSOutput output;
    output.color = float4(0, 1, 0, 1); // Green color
    return output;
}
