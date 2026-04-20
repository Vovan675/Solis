#include "../common.h"
#include "../bindless.h"

cbuffer Uniforms : register(b0)
{
    uint buffer_id;
    uint visible_meshlets_count_buffer_id;
    uint draw_calls_count_buffer_id;
};

static RWByteAddressBuffer visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];
static RWByteAddressBuffer draw_calls_count = ResourceDescriptorHeap[draw_calls_count_buffer_id];

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
#if PERSISTENT_THREADS
    RWStructuredBuffer<TraversalCtrl> traversal_ctrl = ResourceDescriptorHeap[buffer_id];
    traversal_ctrl[0] = (TraversalCtrl)0;
#else
    RWByteAddressBuffer candidate_count = ResourceDescriptorHeap[buffer_id];
    candidate_count.Store(0, 0);
#endif
    visible_meshlets_count.Store(0, 0);
    draw_calls_count.Store(0, 0);
}
