#include "../bindless.h"
#include "culling.h"

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint traversal_queue_buffer_id;
	uint traversal_ctrl_buffer_id;

	uint occluded_instances_buffer_id;
	uint occluded_instances_count_buffer_id;

	uint instances_pass_mask_buffer_id;
	uint instances_count;

	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;
	uint current_pass_mask;
};

static ByteAddressBuffer lod_nodes_buffer = ResourceDescriptorHeap[global_lod_nodes_buffer_id];

static RWStructuredBuffer<TraversalItem> traversal_queue = ResourceDescriptorHeap[traversal_queue_buffer_id];
static RWStructuredBuffer<TraversalCtrl> traversal_ctrl = ResourceDescriptorHeap[traversal_ctrl_buffer_id];

static RWByteAddressBuffer occluded_instances = ResourceDescriptorHeap[occluded_instances_buffer_id];
static RWByteAddressBuffer occluded_instances_count = ResourceDescriptorHeap[occluded_instances_count_buffer_id];

static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

void enqueueRoot(uint instance_id, Mesh mesh)
{
	LodNode root_node = lod_nodes_buffer.Load<LodNode>(sizeof(LodNode) * (mesh.lod_nodes_offset + mesh.root_group_offset));
	bool is_leaf = (root_node.child_count == 0);

	uint dummy;
	InterlockedAdd(traversal_ctrl[0].task_counter, 1, dummy);
	uint base_slot;
	InterlockedAdd(traversal_ctrl[0].write_counter, 1, base_slot);

	TraversalItem item;
	item.instance_id = instance_id;
	if (is_leaf)
		item.packed = packGroupItem(root_node.group_index, root_node.meshlet_count);
	else
		item.packed = packNodeItem(root_node.first_child, root_node.child_count);
	traversal_queue[base_slot] = item;
}

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint thread_id = dispatchID.x;
	uint id;

	#if IS_FIX
		uint occluded_count = occluded_instances_count.Load(0);
		if (thread_id >= occluded_count)
			return;
		id = occluded_instances.Load(thread_id * sizeof(uint));
	#else
		id = thread_id;
		if (id >= instances_count)
			return;

		uint pass_mask = pass_mask_buffer.Load(id * sizeof(uint));
		if ((pass_mask & current_pass_mask) == 0)
			return;
	#endif

	Instance instance = getInstance(id);
	Mesh mesh = getMesh(instance.mesh_id);

	float3 bound_center = instance.bound_center.xyz;
	float3 bound_extent = instance.bound_extent.xyz;
	transformBoundBox(bound_center, bound_extent, instance.world_transform);
	FrustumCullData cull_data = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);

	if (!cull_data.is_visible)
		return;

	// Main would cull against prev-frame hiz, Fix pass would use current frame hiz
	Texture2D hiz_tex = ResourceDescriptorHeap[hiz_tex_id];
	bool is_occluded = isHizOcclusionCulled(cull_data, float2(hiz_width, hiz_height), hiz_mips, hiz_tex);

	if (is_occluded)
	{
		#if !IS_FIX
			// Defer to Fix pass
			uint append_count = WaveActiveCountBits(true);
			uint wave_base = 0;
			if (WaveIsFirstLane())
				occluded_instances_count.InterlockedAdd(0, append_count, wave_base);
			wave_base = WaveReadLaneFirst(wave_base);
			uint slot = wave_base + WavePrefixCountBits(true);
			occluded_instances.Store(slot * sizeof(uint), id);
		#endif
		return;
	}

	enqueueRoot(id, mesh);
}
