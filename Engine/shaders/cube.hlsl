#include "common.h"

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
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
	float4 sun_direction;
	float4 sun_illuminance;
};

float4 PSMain(VertexOutput IN) : SV_TARGET {
    TextureCube texture = ResourceDescriptorHeap[cubemap_tex_id];
    float3 view_direction = normalize(IN.dir);
    float3 color = texture.Sample(linear_wrap_sampler, view_direction).rgb * sky_intensity;
    color += getSunDisk(view_direction, sun_direction.xyz, sun_illuminance.rgb);
    return float4(color, 1.0);
}