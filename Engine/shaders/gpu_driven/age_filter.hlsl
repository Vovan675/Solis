#include "../common.h"
#include "streaming.h"

cbuffer Uniforms : register(b0)
{
	uint group_count;
	uint age_threshold;
	uint group_residency_buffer_id;
	uint stream_requests_buffer_id;
	uint group_ages_buffer_id;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint flat_index = dispatchID.x;
	if (flat_index >= group_count) return;

	RWByteAddressBuffer group_residency = ResourceDescriptorHeap[group_residency_buffer_id];
	RWByteAddressBuffer group_ages = ResourceDescriptorHeap[group_ages_buffer_id];
	RWByteAddressBuffer stream_requests = ResourceDescriptorHeap[stream_requests_buffer_id];

	GroupResidency residency = group_residency.Load<GroupResidency>(flat_index * sizeof(GroupResidency));
	if (residency.geometry_buffer_offset >= GROUP_NON_RESIDENT_ADDRESS_START)
		return;

	uint age = group_ages.Load(flat_index * 4);
	if (age == PINNED_GROUP_AGE) return; // never evict coarsest LOD

	age++;
	group_ages.Store(flat_index * 4, age);

	if (age > age_threshold)
	{
		uint slot;
		stream_requests.InterlockedAdd(4, 1, slot);
		if (slot < MAX_UNLOAD_REQUESTS)
			stream_requests.Store(8 + MAX_STREAMING_REQUESTS * 4 + slot * 4, flat_index);
	}
}
