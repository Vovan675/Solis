#include "streaming.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint candidate_meshlets_buffer_id;
	uint candidate_meshlets_count_buffer_id;
	uint visible_meshlets_buffer_id;
	uint visible_meshlets_count_buffer_id;
	uint draw_indexed_args_buffer_id;
	uint draw_indexed_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;
	uint instances_visibility_buffer_id;
	uint group_residency_buffer_id;
	uint stream_requests_buffer_id;
	uint group_ages_buffer_id;
};

static RWStructuredBuffer<MeshletCandidate> candidate_meshlets_buffer = ResourceDescriptorHeap[candidate_meshlets_buffer_id];
static RWByteAddressBuffer candidate_meshlets_count = ResourceDescriptorHeap[candidate_meshlets_count_buffer_id];
static RWStructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWByteAddressBuffer visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];

#if !USE_MESH_SHADERS
	static RWStructuredBuffer<DrawIndexedIndirect> draw_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
	static RWByteAddressBuffer draw_count = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
	static RWByteAddressBuffer indirect_instances = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
#endif

static RWByteAddressBuffer visibility_buffer = ResourceDescriptorHeap[instances_visibility_buffer_id];
static RWByteAddressBuffer group_residency = ResourceDescriptorHeap[group_residency_buffer_id];
static RWByteAddressBuffer stream_requests = ResourceDescriptorHeap[stream_requests_buffer_id];
static RWByteAddressBuffer group_ages = ResourceDescriptorHeap[group_ages_buffer_id];

void resetAge(uint group_id, uint group_residency_offset)
{
	uint slot = (group_residency_offset + group_id) * 4;
	if (group_ages.Load(slot) != PINNED_GROUP_AGE)
		group_ages.Store(slot, 0);
}

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	if (id >= candidate_meshlets_count.Load<uint>(0)) return;

	MeshletCandidate meshlet_candidate = candidate_meshlets_buffer[id];
	Instance instance = getInstance(meshlet_candidate.instance_id);
	Meshlet meshlet = getMeshlet(meshlet_candidate.meshlet_id, group_residency_buffer_id);
	Mesh mesh = getMesh(instance.mesh_id);

	bool is_visible = true;
	#if FREEZE_CULLING
		is_visible = visibility_buffer.Load(meshlet_candidate.meshlet_id * sizeof(uint));
	#else
		float3 bound_center = meshlet.center.xyz;
		float3 bound_extent = meshlet.extent.xyz;
		transformBoundBox(bound_center, bound_extent, instance.world_transform);
		//addBoundBox(bound_center - bound_extent, bound_center + bound_extent, colorHash(meshlet.group_id));

		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);
		is_visible = cull_data.is_visible;

		visibility_buffer.Store(meshlet_candidate.meshlet_id * sizeof(uint), is_visible);
	#endif

	if (!is_visible) return;

	bool has_refined = (meshlet.refined_group_id != MOST_DETAILED_CLUSTER_GROUP_ID);
	
	float scale = getScaleFromTransform(instance.world_transform);

	bool need_this_level = false;

	bool is_current_resident = isResident(group_residency, mesh.group_residency_offset, meshlet.group_id);
	bool is_refined_resident = isResident(group_residency, mesh.group_residency_offset, meshlet.refined_group_id);

	bool is_refined_coarser = isCoarserThanNeeded(meshlet.refined_group_id, mesh.meshlet_lod_groups_offset, instance.world_transform, scale);
	bool is_current_coarser = isCoarserThanNeeded(meshlet.group_id, mesh.meshlet_lod_groups_offset, instance.world_transform, scale);
	bool is_refined_detailer = !is_refined_coarser;

	if (has_refined && is_refined_resident)
	{
		// If more detailed is too more detailed than needed
		if (is_refined_detailer)
		{
			// If current is less detailed (or exactly) as needed
			need_this_level = is_current_coarser;
		}
	} else if (has_refined)
	{
		// Refined exists but isn't streamed in yet - force render and request it.
		need_this_level = true;
		requestGroup(stream_requests, group_ages, mesh.group_residency_offset, meshlet.refined_group_id);
	} else
	{
		need_this_level = is_current_coarser;
	}

	// If we need this level, but its not resident
	if (need_this_level && !is_current_resident)
	{
		requestGroup(stream_requests, group_ages, mesh.group_residency_offset, meshlet.group_id);
		need_this_level = false;
	}

	// If we are coarser than needed
	if (is_current_coarser)
	{
		resetAge(meshlet.group_id, mesh.group_residency_offset);
	}

	if (!need_this_level) return;
	
	// If we render this, then reset its age
	resetAge(meshlet.group_id, mesh.group_residency_offset);

	uint offset;
	visible_meshlets_count.InterlockedAdd(0, 1, offset);
	visible_meshlets_buffer[offset] = meshlet_candidate;

	#if !USE_MESH_SHADERS
		GroupResidency residency = group_residency.Load<GroupResidency>(sizeof(GroupResidency) * (mesh.group_residency_offset + meshlet.group_id));

		DrawIndexedIndirect cmd;
		cmd.index_count_per_instance = meshlet.getTriangleCount() * 3;
		cmd.instance_count = 1;
		cmd.start_index_location = uint(residency.geometry_buffer_offset + meshlet.triangle_offset) / sizeof(uint);
		cmd.base_vertex_location = 0;
		cmd.start_instance_location = offset;
		draw_args[offset] = cmd;
		indirect_instances.Store(offset * sizeof(uint), offset);
		draw_count.InterlockedAdd(0, 1);
	#endif
}
