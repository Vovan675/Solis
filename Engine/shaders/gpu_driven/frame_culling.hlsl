#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	uint frustums_buffer_id;
	uint frustums_count;
	uint instances_count;
	uint instances_pass_mask_buffer_id;
};

struct FrustumData
{
	float4x4 view_projection;
	uint pass_mask;
	bool is_ortho;
	uint pad[2];
};

static StructuredBuffer<FrustumData> frustums_buffer = ResourceDescriptorHeap[frustums_buffer_id];
static RWByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

uint Cull(float3 bound_center, float3 bound_extent)
{
	uint pass_mask = 0;
	for (int i = 0; i < frustums_count; i++)
	{
		FrustumData frustum = frustums_buffer[i];

		FrustumCullData cull_data;
		if(frustum.is_ortho)
			cull_data = getFrustumCullDataOrtho(bound_center, bound_extent, frustum.view_projection);
		else
			cull_data = getFrustumCullData(bound_center, bound_extent, frustum.view_projection);

		if (cull_data.is_visible)
			pass_mask |= frustum.pass_mask;
	}
	return pass_mask;
}

[numthreads(32, 1, 1)]
void CS_CullInstances(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	
	if (id >= instances_count) return;
	
	Instance instance = getInstance(id);
	Mesh mesh = getMesh(instance.mesh_id);

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);

	uint pass_mask = Cull(bound_center, bound_extent);
	pass_mask_buffer.Store(id * sizeof(uint), pass_mask);
}