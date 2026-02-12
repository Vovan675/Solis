#include "../common.h"
#include "../bindless.h"
#include "culling.h"

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
};

static RWStructuredBuffer<MeshletCandidate> candidate_meshlets_buffer = ResourceDescriptorHeap[candidate_meshlets_buffer_id];
static RWStructuredBuffer<uint> candidate_meshlets_count = ResourceDescriptorHeap[candidate_meshlets_count_buffer_id];
static RWStructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];

#if !USE_MESH_SHADERS
	static RWStructuredBuffer<DrawIndexedIndirect> draw_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
	static RWStructuredBuffer<uint> draw_count = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
	static RWStructuredBuffer<uint> indirect_instances = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
#endif

static RWByteAddressBuffer visibility_buffer = ResourceDescriptorHeap[instances_visibility_buffer_id];

static ByteAddressBuffer lod_groups_buffer = ResourceDescriptorHeap[global_meshlets_lod_groups_buffer_id];

float get_error(float3 center, float radius, float error)
{
	float dist = distance(center, camera_position.xyz) - radius;
	dist = max(dist, z_near);

	return swapchain_size.x * error / (dist * 2.0f * tan((PI / 4.0) * 0.5));
}

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;

	if (id >= candidate_meshlets_count[0]) return;

	MeshletCandidate meshlet_candidate = candidate_meshlets_buffer[id];
	Instance instance = GetInstance(meshlet_candidate.instance_id);
	Meshlet meshlet = GetMeshlet(meshlet_candidate.meshlet_id);
	Mesh mesh = GetMesh(instance.mesh_id);

	bool is_visible = true;

	#if FREEZE_CULLING
		bool was_visible = visibility_buffer.Load(meshlet_candidate.meshlet_id * sizeof(uint));
		is_visible = was_visible;
	#else
		float3 bound_center = meshlet.center.xyz;
		float3 bound_extent = meshlet.extent.xyz;
		transformBoundBox(bound_center, bound_extent, instance.world_transform);

		//addBoundBox(bound_center - bound_extent, bound_center + bound_extent, colorHash(meshlet_candidate.meshlet_id));

		FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);
		is_visible &= cull_data.is_visible;

		visibility_buffer.Store(meshlet_candidate.meshlet_id * sizeof(uint), is_visible);
	#endif

	bool should_draw = is_visible;

	bool is_lod_selected = true;

	// cluster should be rendered if:
	// 1. clodGroup::simplified for the group it's in is over error threshold
	// 2. cluster.refined is -1 *or* clodGroup::simplified for groups[cluster.refined].simplified is at or under error threshold
	
	LODGroup group = lod_groups_buffer.Load<LODGroup>(sizeof(LODGroup) * (meshlet.group_id + mesh.meshlet_lod_groups_offset)); 
	transformBoundSphere(group.center, group.radius, instance.world_transform);

	const float error_threshold = 1.0;
	
	float scale = getScaleFromTransform(instance.world_transform);

	//addBoundBox(group.center - group.radius.xxx, group.center + group.radius.xxx, colorHash(meshlet_candidate.meshlet_id));
	float group_error = get_error(group.center, group.radius, group.error * scale);

	if (meshlet.parent_id == -1)
	{
		is_lod_selected = group_error > error_threshold;
	} else
	{
		LODGroup parent_group = lod_groups_buffer.Load<LODGroup>(sizeof(LODGroup) * (meshlet.parent_id + mesh.meshlet_lod_groups_offset)); 
		transformBoundSphere(parent_group.center, parent_group.radius, instance.world_transform);
		
		float parent_group_error = get_error(parent_group.center, parent_group.radius, parent_group.error * scale);

		is_lod_selected = (group_error > error_threshold && parent_group_error <= error_threshold);
	}

	should_draw &= is_lod_selected;

	if (should_draw)
	{
		uint offset;
		InterlockedAdd(visible_meshlets_count[0], 1, offset);

		visible_meshlets_buffer[offset] = meshlet_candidate;

		#if !USE_MESH_SHADERS
			DrawIndexedIndirect cmd;
			cmd.index_count_per_instance = meshlet.triangle_count * 3;
			cmd.instance_count = 1;
			cmd.start_index_location = meshlet.triangle_offset;
			cmd.base_vertex_location = 0;
			cmd.start_instance_location = offset;

			draw_args[offset] = cmd;
			indirect_instances[offset] = offset;

			InterlockedAdd(draw_count[0], 1);
		#endif
	}
}