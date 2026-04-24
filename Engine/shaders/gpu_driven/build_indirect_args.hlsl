#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	uint count_buffer_id;
	uint dispatch_indirect_args_buffer_id;
	uint group_size;
};

static ByteAddressBuffer count_buffer = ResourceDescriptorHeap[count_buffer_id];
static RWStructuredBuffer<DispatchIndirect> dispatch_args = ResourceDescriptorHeap[dispatch_indirect_args_buffer_id];

[numthreads(1, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint count = count_buffer.Load(0);
	uint num_groups = (count + group_size - 1) / group_size;
	dispatch_args[0].group = uint3(num_groups, 1, 1);
}