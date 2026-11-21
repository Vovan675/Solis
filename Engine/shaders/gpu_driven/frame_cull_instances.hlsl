#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	uint cull_views_buffer_id;
	uint cull_views_count;
	uint instances_count;
	uint instances_pass_mask_buffer_id;
};

static StructuredBuffer<CullView> cull_views = ResourceDescriptorHeap[cull_views_buffer_id];
static RWByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

uint Cull(float3 bound_center, float3 bound_extent)
{
	uint pass_mask = 0;
	for (int i = 0; i < cull_views_count; i++)
	{
		CullView cull_view = cull_views[i];
		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, cull_view);
		if (cull_data.is_visible)
			pass_mask |= cull_view.pass_mask;
	}
	return pass_mask;
}

[numthreads(32, 1, 1)]
void CS_CullInstances(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	
	if (id >= instances_count) return;

	Instance instance = getInstance(id);

	if (instance.flags & INSTANCE_FLAG_INVALID)
	{
		pass_mask_buffer.Store(id * sizeof(uint), 0);
		return;
	}

	Mesh mesh = getMesh(instance.mesh_id);

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);

	uint pass_mask = Cull(bound_center, bound_extent);
	pass_mask &= instance.pass_mask;
	pass_mask_buffer.Store(id * sizeof(uint), pass_mask);
}