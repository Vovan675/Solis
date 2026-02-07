#include "../common.h"
#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;
	uint visible_instances_buffer_id;
	uint visible_instances_count_buffer_id;
	uint visible_meshlets_buffer_id;
	uint visible_meshlets_count_buffer_id;
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
	uint pad[3];
};

static RWStructuredBuffer<MeshletCandidate> visible_instances_buffer = ResourceDescriptorHeap[visible_instances_buffer_id];
static RWStructuredBuffer<uint> visible_instances_count = ResourceDescriptorHeap[visible_instances_count_buffer_id];

static RWStructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];


static RWStructuredBuffer<DrawIndexedIndirect> draw_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
static RWStructuredBuffer<uint> draw_count  = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
static RWStructuredBuffer<uint> indirect_instances  = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
static RWByteAddressBuffer visibility_buffer = ResourceDescriptorHeap[instances_visibility_buffer_id];
static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

[numthreads(32, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;

	if (id >= visible_instances_count[0]) return;

	MeshletCandidate meshlet_candidate = visible_instances_buffer[id];
	Instance instance = GetInstance(meshlet_candidate.instance_id);
	Mesh mesh = GetMesh(instance.mesh_id);
	Meshlet meshlet = GetMeshlet(meshlet_candidate.meshlet_id);

	uint local_meshlet_id = meshlet_candidate.meshlet_id - mesh.meshlet_offset;
	uint visibility_key = (meshlet_candidate.instance_id * 512) + local_meshlet_id;

	bool is_visible = true;

	#if FREEZE_CULLING
		bool was_visible = visibility_buffer.Load(meshlet_candidate.meshlet_id * sizeof(uint));
		is_visible = was_visible;
	#else
		float3 bound_center = meshlet.center.xyz;
		//float3 bound_extent = meshlet.radius.xxx;
		float3 bound_extent = meshlet.extent.xyz;
		transformBoundBox(bound_center, bound_extent, instance.world_transform);
		float3 color = hash31(meshlet_candidate.meshlet_id);
		//addBoundBox(bound_center - bound_extent, bound_center + bound_extent, color);
		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);

		is_visible &= cull_data.is_visible;

		if (is_visible)
		{
			//float2 hiz_size = float2(hiz_width, hiz_height);
			//Texture2D hiz_tex = ResourceDescriptorHeap[hiz_tex_id];
			//is_visible &= !isHIZOcclusionCulled(cull_data, hiz_size, hiz_mips, hiz_tex);
		}

		visibility_buffer.Store(meshlet_candidate.meshlet_id * sizeof(uint), is_visible);
	#endif

	bool should_draw = is_visible;

	if (should_draw)
	{
		uint offset;
		InterlockedAdd(draw_count[0], 1, offset);

		DrawIndexedIndirect cmd;
		cmd.index_count_per_instance = meshlet.triangle_count * 3;
		cmd.instance_count = 1;
		cmd.start_index_location = meshlet.triangle_offset;
		cmd.base_vertex_location = 0;
		cmd.start_instance_location = offset;

		draw_args[offset] = cmd;
		indirect_instances[offset] = id;

		uint meshlet_offset;
		InterlockedAdd(visible_meshlets_count[0], 1, meshlet_offset);
		visible_meshlets_buffer[meshlet_offset] = meshlet_candidate;
	}
}