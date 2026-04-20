#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;
	uint draw_indexed_args_buffer_id;
	uint draw_indexed_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;
	uint instances_pass_mask_buffer_id;
	uint instances_count;
	uint current_pass_mask;
	uint pad[2];
};

static RWStructuredBuffer<DrawIndexedIndirect> draw_indexed_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
static RWStructuredBuffer<uint> draw_indexed_count  = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
static RWStructuredBuffer<uint> indirect_instances  = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

/*
bool isSphereInFrustum(float3 center, float radius)
{
	for (int i = 0; i < 6; i++)
	{
		float d = dot(float4(center, 1), frustum_planes[i]);
		if (d < -radius) return false;
	}
	return true;
}
*/

#define HIZ_OCCLUSION_DEBUG 1
[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	
	if (id >= instances_count) return;
	
	uint pass_mask = pass_mask_buffer.Load<uint>(id * sizeof(uint));

	if ((pass_mask & current_pass_mask) == 0) return;

	Instance instance = getInstance(id);
	Mesh mesh = getMesh(instance.mesh_id);

	bool is_visible = true;

	// frustum cull for every stage
	//float3 bs_center = instance.bound_sphere.xyz;
	//float bs_radius = instance.bound_sphere.a;
	//is_visible &= isSphereInFrustum(bs_center, bs_radius);

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);

	#if IS_ORTHO_FRUSTUM
		FrustumCullData cull_data = getFrustumCullDataOrtho(bound_center, bound_extent, frustum_view_projection);
	#else
		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);
	#endif
	is_visible &= cull_data.is_visible;
	
	bool should_draw = is_visible;
	if (should_draw)
	{
		uint visible_count = WaveActiveSum(1);

		uint base_offset;

		if (WaveIsFirstLane())
			InterlockedAdd(draw_indexed_count[0], visible_count, base_offset);

		base_offset = WaveReadLaneFirst(base_offset);
		uint offset = base_offset + WavePrefixSum(1);

		DrawIndexedIndirect cmd;
		cmd.index_count_per_instance = mesh.indices_count;
		cmd.instance_count = 1;
		cmd.start_index_location = mesh.index_offset;
		cmd.base_vertex_location = 0;
		cmd.start_instance_location = offset;
		draw_indexed_args[offset] = cmd;
		indirect_instances[offset] = id;
	}
}