#include "meshlet_common.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint visible_meshlets_buffer_id;
	uint visible_meshlets_count_buffer_id;
	uint draw_indexed_args_buffer_id;
	uint draw_indexed_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;

	uint occluded_meshlets_buffer_id;
	uint occluded_meshlets_count_buffer_id;

	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;

	uint group_residency_buffer_id;
};

static RWStructuredBuffer<MeshletCandidate> visible_meshlets = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];
static RWStructuredBuffer<MeshletCandidate> occluded_meshlets = ResourceDescriptorHeap[occluded_meshlets_buffer_id];
static RWByteAddressBuffer occluded_meshlets_count = ResourceDescriptorHeap[occluded_meshlets_count_buffer_id];

#if !USE_MESH_SHADERS
	static RWStructuredBuffer<DrawIndexedIndirect> draw_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
	static RWByteAddressBuffer draw_count = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
	static RWByteAddressBuffer indirect_instances = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
#endif

static RWByteAddressBuffer group_residency = ResourceDescriptorHeap[group_residency_buffer_id];

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID)
{
	uint id = dispatch_id.x;
	uint count = occluded_meshlets_count.Load(0);
	if (id >= count)
		return;

	MeshletCandidate candidate = occluded_meshlets[id];
	Instance instance = getInstance(candidate.instance_id);
	Mesh mesh = getMesh(instance.mesh_id);
	uint residency_base = mesh.group_residency_offset;

	Meshlet meshlet = getMeshlet(candidate.meshlet_id, group_residency_buffer_id);

	HizParams hiz = { hiz_tex_id, hiz_width, hiz_height, hiz_mips };
	FrustumCullData cull;
	if (!cullMeshletVisibility(meshlet, instance.world_transform, frustum_view_projection, hiz, cull))
		return;

	uint slot = appendVisibleMeshlet(candidate, visible_meshlets, visible_meshlets_count);
	#if !USE_MESH_SHADERS
		writeIndirectDraw(slot, meshlet, residency_base, group_residency, draw_args, draw_count, indirect_instances);
	#endif
}
