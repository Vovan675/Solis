#include "common.h"

cbuffer Uniforms : register(b0)
{
    uint output_tex_id;
    uint depth_tex_id;
    int2 texture_size;
    uint convert_reverse_z; // for first mip converting
};

static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];
static Texture2D depth_texture = ResourceDescriptorHeap[depth_tex_id];

// HiZ always stores standard-z
[numthreads(THREADGROUP_SIZE, THREADGROUP_SIZE, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    uint3 coord = dispatchID;

    if (coord.x >= texture_size.x || coord.y >= texture_size.y) return;

    float2 uv = (coord.xy + float2(0.5, 0.5)) / float2(texture_size);
    float4 gather = depth_texture.Gather(point_clamp_sampler, uv);
    if (convert_reverse_z)
        gather = 1.0 - gather;
    float result = max(gather.x, max(gather.y, max(gather.z, gather.w)));

    output_texture[coord.xy] = result;
}