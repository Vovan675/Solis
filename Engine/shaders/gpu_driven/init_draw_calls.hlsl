#include "../common.h"

cbuffer Uniforms : register(b0)
{
    uint draw_calls_count_buffer_id;
};

static RWStructuredBuffer<uint> indirect_args_count  = ResourceDescriptorHeap[draw_calls_count_buffer_id];

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    indirect_args_count[0] = 0;
}