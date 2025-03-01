#include "../common.h"

struct VS_INPUT
{
    float4 inPos : POSITION;
};

struct VS_OUTPUT
{
    float4 outPos : SV_POSITION;
    float4 worldPos : TEXCOORD0;
};

cbuffer PushConstants : register(b0)
{
    matrix model;
};

cbuffer UBO : register(b1)
{
    matrix light_space_matrix;
    float4 light_pos;
    float z_far;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    output.outPos = mul(light_space_matrix, mul(model, input.inPos));
    output.worldPos = mul(model, input.inPos);
    return output;
}

#if LIGHT_TYPE == 0
    float PSMain(VS_OUTPUT input) : SV_Depth
    {
        float lightDistance = length(input.worldPos.xyz - light_pos.xyz);
        return lightDistance / z_far;
    }
#elif LIGHT_TYPE == 1
    void PSMain(VS_OUTPUT input)
    {
    }
#endif
