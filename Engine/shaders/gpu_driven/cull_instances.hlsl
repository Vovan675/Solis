#include "../bindless.h"
#include "culling.h"

static ByteAddressBuffer lod_nodes_buffer = ResourceDescriptorHeap[global_lod_nodes_buffer_id];

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint traversal_queue_buffer_id;
	uint traversal_ctrl_buffer_id;

	uint instances_visibility_buffer_id;
	uint instances_pass_mask_buffer_id;
	uint instances_count;
	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;
	uint current_pass_mask;
};

static RWStructuredBuffer<TraversalItem> traversal_queue = ResourceDescriptorHeap[traversal_queue_buffer_id];
static RWStructuredBuffer<TraversalCtrl> traversal_ctrl = ResourceDescriptorHeap[traversal_ctrl_buffer_id];

static RWByteAddressBuffer visibility_buffer = ResourceDescriptorHeap[instances_visibility_buffer_id];
static ByteAddressBuffer pass_mask_buffer = ResourceDescriptorHeap[instances_pass_mask_buffer_id];

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
	uint id = dispatchID.x;
	if (id >= instances_count) return;

	uint pass_mask = pass_mask_buffer.Load<uint>(id * sizeof(uint));
	if ((pass_mask & current_pass_mask) == 0) return;

	Instance instance = getInstance(id);
	Mesh mesh = getMesh(instance.mesh_id);

	bool was_visible = visibility_buffer.Load(id * sizeof(uint));
	bool is_visible = true;

	#if !IS_LATE
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
				is_visible &= !isHizOcclusionCulled(cull_data, hiz_size, hiz_mips, hiz_tex);
			}
			visibility_buffer.Store(id * sizeof(uint), is_visible);
		#endif
	#endif

	bool should_draw = is_visible;
	#if IS_LATE
		should_draw = is_visible && !was_visible;
	#endif

	if (!should_draw) return;

	LodNode root_node = lod_nodes_buffer.Load<LodNode>(sizeof(LodNode) * (mesh.lod_nodes_offset + mesh.root_group_offset));

	bool is_leaf = (root_node.child_count == 0);

	uint dummy;
	InterlockedAdd(traversal_ctrl[0].task_counter, 1, dummy);
	uint base_slot;
	InterlockedAdd(traversal_ctrl[0].write_counter, 1, base_slot);

	TraversalItem item;
	item.instance_id = id;

	// Leafs are appended as groups
	if (is_leaf)
		item.packed = packGroupItem(root_node.group_index, root_node.meshlet_count);
	else
		item.packed = packNodeItem(root_node.first_child, root_node.child_count);
	traversal_queue[base_slot] = item;
}
