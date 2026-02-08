#include "../common.h"

cbuffer Uniforms : register(b0)
{
    uint candidate_meshlets_count_buffer_id;
    uint visible_meshlets_count_buffer_id;
    uint draw_calls_count_buffer_id;
};

static RWStructuredBuffer<uint> candidate_meshlets_count = ResourceDescriptorHeap[candidate_meshlets_count_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];
static RWStructuredBuffer<uint> indirect_args_count = ResourceDescriptorHeap[draw_calls_count_buffer_id];

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    candidate_meshlets_count[0] = 0;
    visible_meshlets_count[0] = 0;
    indirect_args_count[0] = 0;
}