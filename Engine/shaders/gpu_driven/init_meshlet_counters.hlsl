#include "../common.h"
#include "../bindless.h"

cbuffer Uniforms : register(b0)
{
	uint traversal_ctrl_buffer_id;
	uint visible_meshlets_count_buffer_id;
	uint draw_calls_count_buffer_id;
	uint occluded_meshlets_count_buffer_id;
	uint occluded_instances_count_buffer_id;
	uint reset_occluded;
};

static RWStructuredBuffer<TraversalCtrl> traversal_ctrl = ResourceDescriptorHeap[traversal_ctrl_buffer_id];
static RWByteAddressBuffer visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];
static RWByteAddressBuffer draw_calls_count = ResourceDescriptorHeap[draw_calls_count_buffer_id];
static RWByteAddressBuffer occluded_meshlets_count = ResourceDescriptorHeap[occluded_meshlets_count_buffer_id];
static RWByteAddressBuffer occluded_instances_count = ResourceDescriptorHeap[occluded_instances_count_buffer_id];

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	traversal_ctrl[0] = (TraversalCtrl)0;
	visible_meshlets_count.Store(0, 0);
	draw_calls_count.Store(0, 0);
	if (reset_occluded != 0)
	{
		occluded_meshlets_count.Store(0, 0);
		occluded_instances_count.Store(0, 0);
	}
}
