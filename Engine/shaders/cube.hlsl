#include "common.h"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 color : COLOR;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float3 dir : TEXCOORD0;
};

VertexOutput VSMain(VertexInput IN) {
    VertexOutput OUT;

    // Skybox rendering (remove translation from view matrix)
    //float4x4 viewNoTranslation = RemoveTranslation(view);
    
    // Transform position
    OUT.position = mul(projection, mul(view, float4(IN.position + camera_position.xyz, 1.0)));
    OUT.position.z = 0;
    OUT.dir = normalize(IN.position);
    return OUT;
}

cbuffer Constants : register(b0)
{
	uint cubemap_tex_id;
};

float4 PSMain(VertexOutput IN) : SV_TARGET {
    TextureCube texture = ResourceDescriptorHeap[cubemap_tex_id];
    float3 color = texture.Sample(linear_wrap_sampler, IN.dir).rgb;
    return float4(color, 1.0);
}