#include "common.h"

struct VSInput {
    float2 uv : TEXCOORD0;
};

struct PSOutput {
    float4 color : SV_Target;
};

PSOutput PSMain(VSInput input)
{
    PSOutput output;
    float2 uv = input.uv;
    output.color = float4(1, 0, 0, 1);
    return output;
}
