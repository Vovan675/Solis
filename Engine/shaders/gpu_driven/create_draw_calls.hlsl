#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;
	uint draw_indexed_args_buffer_id;
	uint draw_indexed_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;
	uint instances_visibility_buffer_id;
	uint instances_pass_mask_buffer_id;
	uint instances_count;
	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;
	uint current_pass_mask;
	uint pad;
};

static RWStructuredBuffer<DrawIndexedIndirect> draw_indexed_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
static RWStructuredBuffer<uint> draw_indexed_count  = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
static RWStructuredBuffer<uint> indirect_instances  = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
static RWByteAddressBuffer visibility_buffer = ResourceDescriptorHeap[instances_visibility_buffer_id];
static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	
	if (id >= instances_count) return;
	
	uint pass_mask = pass_mask_buffer.Load<uint>(id * sizeof(uint));

	if ((pass_mask & current_pass_mask) == 0) return;

	Instance instance = GetInstance(id);
	Mesh mesh = GetMesh(instance.mesh_id);

	bool was_visible = visibility_buffer.Load(id * sizeof(uint));
	bool is_visible = true;

	#if IS_LATE == 0
		is_visible = was_visible;
	#endif

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);

	FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);

	is_visible &= cull_data.is_visible;
	
	#if FREEZE_CULLING
		is_visible = was_visible;
	#else
		#if IS_LATE
			if (is_visible)
			{
				float2 hiz_size = float2(hiz_width, hiz_height);
    			Texture2D hiz_tex = ResourceDescriptorHeap[hiz_tex_id];
				is_visible &= !isHIZOcclusionCulled(cull_data, hiz_size, hiz_mips, hiz_tex);
			}
			visibility_buffer.Store(id * sizeof(uint), is_visible);
		#endif
	#endif

	bool should_draw = is_visible;
	#if IS_LATE
		should_draw = is_visible && !was_visible;
	#endif
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