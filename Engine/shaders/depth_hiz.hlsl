#include "common.h"

cbuffer Uniforms : register(b0)
{
    uint output_tex_id;
    uint depth_tex_id;
    int2 texture_size;
};

static RWTexture2D<float> output_texture  = ResourceDescriptorHeap[output_tex_id];
static Texture2D depth_texture = ResourceDescriptorHeap[depth_tex_id];

[numthreads(32, 32, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    uint3 coord = dispatchID;
    
    if (coord.x >= texture_size.x || coord.y >= texture_size.y) return;
    
    float4 gather = depth_texture.Gather(point_clamp_sampler, (coord.xy + float2(0.5, 0.5)) / float2(texture_size));
    float result = min(gather.x, min(gather.y, min(gather.z, gather.w)));

    output_texture[coord.xy] = result;
}