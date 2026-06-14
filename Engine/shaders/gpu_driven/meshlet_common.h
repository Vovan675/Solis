#pragma once
#include "streaming.h"

struct HizParams
{
	uint tex_id;
	uint width;
	uint height;
	uint mips;
};

bool cullMeshletVisibility(
	Meshlet meshlet,
	float4x4 world_transform,
	float4x4 frustum_view_projection,
	HizParams hiz,
	out FrustumCullData cull)
{
	float3 bound_center = meshlet.center.xyz;
	float3 bound_extent = meshlet.extent.xyz;
	transformBoundBox(bound_center, bound_extent, world_transform);
	#if IS_ORTHO_FRUSTUM
		cull = getFrustumCullDataOrtho(bound_center, bound_extent, frustum_view_projection);
	#else
		cull = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);
	#endif

	if (!cull.is_visible)
		return false;

	#if USE_OCCLUSION
		Texture2D hiz_tex = ResourceDescriptorHeap[hiz.tex_id];
		return !isHizOcclusionCulled(cull, float2(hiz.width, hiz.height), hiz.mips, hiz_tex);
	#else
		return true;
	#endif
}

uint appendVisibleMeshlet(
	MeshletCandidate candidate,
	RWStructuredBuffer<MeshletCandidate> visible_meshlets,
	RWStructuredBuffer<uint> visible_meshlets_count)
{
	uint slot;
	InterlockedAdd(visible_meshlets_count[0], 1, slot);
	visible_meshlets[slot] = candidate;
	return slot;
}

void writeIndirectDraw(
	uint slot,
	Meshlet meshlet,
	uint residency_base,
	RWByteAddressBuffer group_residency,
	RWStructuredBuffer<DrawIndexedIndirect> draw_args,
	RWByteAddressBuffer draw_count,
	RWByteAddressBuffer indirect_instances)
{
	GroupResidency residency = group_residency.Load<GroupResidency>(sizeof(GroupResidency) * (residency_base + meshlet.group_id));

	DrawIndexedIndirect cmd;
	cmd.index_count_per_instance = meshlet.getTriangleCount() * 3;
	cmd.instance_count = 1;
	cmd.start_index_location = uint(residency.geometry_buffer_offset + meshlet.triangle_offset) / sizeof(uint);
	cmd.base_vertex_location = 0;
	cmd.start_instance_location = slot;
	draw_args[slot] = cmd;
	indirect_instances.Store(slot * sizeof(uint), slot);
	draw_count.InterlockedAdd(0, 1);
}

void appendOccluded(
	MeshletCandidate candidate,
	bool do_append,
	RWStructuredBuffer<MeshletCandidate> occluded_meshlets,
	RWByteAddressBuffer occluded_meshlets_count)
{
	uint append_count = WaveActiveCountBits(do_append);
	uint wave_base = 0;
	if (WaveIsFirstLane() && append_count > 0)
		occluded_meshlets_count.InterlockedAdd(0, append_count, wave_base);
	wave_base = WaveReadLaneFirst(wave_base);
	uint slot = wave_base + WavePrefixCountBits(do_append);

	if (do_append)
		occluded_meshlets[slot] = candidate;
}
