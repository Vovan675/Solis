#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint draw_args_buffer_id;
	uint draw_count_buffer_id;
	uint draw_instances_buffer_id;

	uint instances_pass_mask_buffer_id;
	uint instances_count;
	uint current_pass_mask;
};

static RWStructuredBuffer<DrawIndirect> draw_args = ResourceDescriptorHeap[draw_args_buffer_id];
static RWByteAddressBuffer draw_count = ResourceDescriptorHeap[draw_count_buffer_id];
static RWByteAddressBuffer draw_instances = ResourceDescriptorHeap[draw_instances_buffer_id];
static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	if (id >= instances_count)
		return;

	uint pass_mask = pass_mask_buffer.Load(id * sizeof(uint));
	if ((pass_mask & current_pass_mask) == 0)
		return;

	Instance instance = getInstance(id);
	Mesh mesh = getMesh(instance.mesh_id);

	if ((mesh.flags & MESH_FLAG_MESHLET) != 0)
		return;

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);
	#if IS_ORTHO_FRUSTUM
		FrustumCullData cull_data = getFrustumCullDataOrtho(bound_center, bound_extent, frustum_view_projection);
	#else
		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);
	#endif

	if (!cull_data.is_visible)
		return;

	uint slot;
	draw_count.InterlockedAdd(0, 1, slot);

	DrawIndirect cmd;
	cmd.vertex_count_per_instance = mesh.indices_count;
	cmd.instance_count = 1;
	cmd.first_vertex = 0;
	cmd.first_instance = slot;
	draw_args[slot] = cmd;

	draw_instances.Store(slot * sizeof(uint), id);
}
