#include "meshlet_common.h"
// Reference: NVIDIA vk_lod_clusters

cbuffer Uniforms : register(b0)
{
	uint view_id;
	uint cull_views_buffer_id;

	uint queue_buffer_id;
	uint traversal_ctrl_buffer_id;

	uint visible_meshlets_buffer_id;
	uint visible_meshlets_count_buffer_id;
	uint draw_args_buffer_id;
	uint draw_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;
	uint max_queue_size;

	uint occluded_meshlets_buffer_id;
	uint occluded_meshlets_count_buffer_id;

	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;

	uint page_table_buffer_id;
	uint stream_requests_buffer_id;
	uint page_frames_buffer_id;
};

static StructuredBuffer<CullView> cull_views = ResourceDescriptorHeap[cull_views_buffer_id];
DECLARE_COHERENT_RW_STRUCTURED_BUFFER(queue, TraversalItem, queue_buffer_id)
DECLARE_COHERENT_RW_STRUCTURED_BUFFER(ctrl, TraversalCtrl, traversal_ctrl_buffer_id)
static RWStructuredBuffer<MeshletCandidate> visible_meshlets = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];
static RWStructuredBuffer<MeshletCandidate> occluded_meshlets = ResourceDescriptorHeap[occluded_meshlets_buffer_id];
static RWByteAddressBuffer occluded_meshlets_count = ResourceDescriptorHeap[occluded_meshlets_count_buffer_id];

#if !USE_MESH_SHADERS
	static RWStructuredBuffer<DrawIndirect> draw_args = ResourceDescriptorHeap[draw_args_buffer_id];
	static RWByteAddressBuffer draw_count = ResourceDescriptorHeap[draw_count_buffer_id];
	static RWByteAddressBuffer indirect_instances = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
#endif

static ByteAddressBuffer page_table = ResourceDescriptorHeap[page_table_buffer_id];
static RWByteAddressBuffer stream_requests = ResourceDescriptorHeap[stream_requests_buffer_id];
static RWByteAddressBuffer page_frames = ResourceDescriptorHeap[page_frames_buffer_id];

bool requestGroupPages(uint page_table_offset, LODGroup group)
{
	bool is_resident = true;
	uint first_page = page_table_offset + unpackLocationPage(group.meshlets_location);
	for (uint page = first_page; page < first_page + group.page_count; page++)
	{
		uint last_frame = page_frames.Load(page * 4);
		if (last_frame != frame)
			page_frames.InterlockedMax(page * 4, frame, last_frame); // mark page as used this frame (for LRU eviction)
		if (page_table.Load(page * 4) != MESHLET_PAGE_NOT_RESIDENT)
			continue;

		// Add stream request for non resident page (deduplicate by frame)
		is_resident = false;
		if (last_frame != frame)
		{
			uint slot;
			stream_requests.InterlockedAdd(0, 1, slot);
			if (slot < MAX_STREAMING_REQUESTS)
				stream_requests.Store(4 + slot * 4, page);
		}
	}
	return is_resident;
}

// Processes one child.
// All queue writes are wave batched
// There are two types of items:
// 1) Node Item: child is also node, or if leaf then meshlet group
// 2) Group Item: child is meshlet. Find exact DAG cut and append draw calls
void processSubTask(TraversalItem item, uint sub_id, bool is_valid)
{
	bool is_group = item.isGroup();

	Instance instance = getInstance(item.instance_id);
	Mesh mesh = getMesh(instance.mesh_id);

	float scale = getScaleFromTransform(instance.world_transform);

	// NODE PATH - per child BVH node: descend further, enqueue as LOD group, or fire a streaming request.
	if (!is_group)
	{
		uint child_offset = item.getNodeChildOffset();
		LodNode child_node = lod_nodes_buffer.Load<LodNode>(sizeof(LodNode) * (mesh.lod_nodes_offset + child_offset + sub_id));

		// If child too coarse we must go deeper
		bool is_child_too_coarse = isCoarserThanNeeded(child_node.center, child_node.radius, child_node.error, instance.world_transform, scale);
		bool is_leaf = (child_node.child_count == 0);

		// Nodes are just BVH descriptions, no real meshlets, just for traversal
		bool descend_node = is_valid && is_child_too_coarse && !is_leaf;

		// Leaves map to real meshlet groups
		bool enqueue_group = is_valid && is_child_too_coarse && is_leaf;

		// Check if meshlet group is resident, if not dont render it and make streaming request
		if (enqueue_group)
			enqueue_group = requestGroupPages(mesh.page_table_offset, getLODGroup(mesh.meshlet_lod_groups_offset, child_node.group_index));

		uint node_count = WaveActiveCountBits(descend_node);
		uint group_count = WaveActiveCountBits(enqueue_group);

		uint node_base = 0;
		uint group_base = 0;

		if (WaveIsFirstLane())
		{
			uint dummy;
			InterlockedAdd(ctrl[0].task_counter, node_count + group_count, dummy);
			InterlockedAdd(ctrl[0].write_counter, node_count + group_count, node_base);
			group_base = node_base + node_count;
		}

		DeviceMemoryBarrier();

		uint node_slot = WaveReadLaneFirst(node_base) + WavePrefixCountBits(descend_node);
		uint group_slot = WaveReadLaneFirst(group_base) + WavePrefixCountBits(enqueue_group);

		bool write_node = descend_node && node_slot < max_queue_size;
		bool write_group = enqueue_group && group_slot < max_queue_size;

		TraversalItem out_item;
		out_item.instance_id = item.instance_id;

		if (write_node)
		{
			out_item.packed = packNodeItem(child_node.first_child, child_node.child_count);
			queue[node_slot] = out_item;
		}

		if (write_group)
		{
			out_item.packed = packGroupItem(child_node.group_index, child_node.meshlet_count);
			queue[group_slot] = out_item;
		}

		DeviceMemoryBarrier();
	}
	// GROUP PATH - children are meshlets
	else
	{
		LODGroup group = getLODGroup(mesh.meshlet_lod_groups_offset, item.getGroupIndex());
		uint meshlet_address = getPageAddress(page_table, mesh.page_table_offset, group.meshlets_location) + sub_id * sizeof(Meshlet);
		Meshlet meshlet = getMeshlet(meshlet_address);

		HizParams hiz = { hiz_tex_id, hiz_width, hiz_height, hiz_mips };
		FrustumCullData cull;
		bool is_occluded = !cullMeshletVisibility(meshlet, instance.world_transform, cull_views[view_id], hiz, cull);
		bool is_visible = is_valid && cull.is_visible;

		// Pick this meshlet when:
		// 1) The finer (refined) group would be wasteful - going deeper isn't worth it, cut lands here.
		// 2) The finer group doesn't exist (finest LOD) or isn't resident - force render, no alternative.
		bool need_this_level = true; // If have no finer group or its not resident then render it
		if (!meshlet.isMostDetailed())
		{
			LODGroup refined_group = getLODGroup(mesh.meshlet_lod_groups_offset, meshlet.refined_group_id);
			if (isResident(page_table, mesh.page_table_offset, refined_group))
				need_this_level = !isCoarserThanNeeded(refined_group.center, refined_group.radius, refined_group.error, instance.world_transform, scale);
		}

		bool selected = is_visible && need_this_level;

		MeshletCandidate candidate;
		candidate.instance_id = item.instance_id;
		candidate.meshlet_address = meshlet_address;

		if (selected && !is_occluded)
		{
			uint slot = appendVisibleMeshlet(candidate, visible_meshlets, visible_meshlets_count);
			#if !USE_MESH_SHADERS
				writeIndirectDraw(slot, meshlet, draw_args, draw_count, indirect_instances);
			#endif
		}
		#if !IS_FIX
			// Defer to Meshlet Flat Fix pass
			appendOccluded(candidate, selected && is_occluded, occluded_meshlets, occluded_meshlets_count);
		#endif
	}
}

// Distributes all sub-tasks from active threads across the wave in packed batches
// 
// Example of pool SIMT utilization:
// T0 T1 T2 (4 2 1 children)
// ----------
// A0 B0 C0
// A1 B1
// A2
// A3
// poor utilization visible: 3 iterations, T2 idle after 1
//
// Packed across wave:
// T0 T1 T2 T3 T4 T5 T6
// ----------------------
// A0 A1 A2 A3 B0 B1 C0
// good utilization: 1 iteration, all lanes do work

groupshared uint shared_tasks[THREADGROUP_SIZE];

void processAllSubTasks(TraversalItem item, uint thread_sub_count)
{
	uint lane_index = WaveGetLaneIndex();
	uint lane_count = WaveGetLaneCount();

	int start_offset = WavePrefixSum(thread_sub_count);
	int end_offset = start_offset + thread_sub_count;
	int total_threads = WaveReadLaneAt(end_offset, lane_count - 1);
	int total_runs = divideRoundUp(total_threads, lane_count);

	bool has_task = thread_sub_count > 0;
	uint task_offset = WavePrefixCountBits(has_task);

	if (has_task)
		shared_tasks[task_offset] = lane_index;

	GroupMemoryBarrierWithGroupSync();

	int task_base = -1;
	for (int run_index = 0; run_index < total_runs; run_index++)
	{
		// Each run covers [slot_first, slot_first + lane_count]
		// Each lane covers exactly one slot (task)
		int slot_first = run_index * lane_count;
		int slot = slot_first + lane_index;

		// Challenge is to find which task owns this slot (magic from nvidia sample):
		int rel_start_in_run = start_offset - slot_first;
		bool task_starts_in_this_run = has_task
			&& rel_start_in_run >= 0
			&& rel_start_in_run < lane_count;

		uint my_start_bit = task_starts_in_this_run ? (1u << rel_start_in_run) : 0u;
		uint task_starts_mask = WaveActiveBitOr(my_start_bit);

		uint bits_up_to_me = (lane_index >= 31)
							? 0xFFFFFFFF
							: ((1u << (lane_index + 1)) - 1);
		int task = countbits(task_starts_mask & bits_up_to_me) + task_base;

		uint source_lane = shared_tasks[task];

		uint task_sub_id = slot - WaveReadLaneAt(start_offset, source_lane);
		uint task_sub_count = WaveReadLaneAt(thread_sub_count, source_lane);

		task_base = WaveReadLaneAt(task, lane_count - 1); // for next iteration

		TraversalItem source_item;
		source_item.instance_id = WaveReadLaneAt(item.instance_id, source_lane);
		source_item.packed = WaveReadLaneAt(item.packed, source_lane);

		bool slot_valid = slot < total_threads;
		processSubTask(source_item, slot_valid ? min(task_sub_id, task_sub_count - 1) : 0, slot_valid);
	}
}

// Persistent-thread producer/consumer loop.
void run()
{
	uint thread_read_index = ~0u;

	while (true)
	{
		// If entire wave has no work, acquire new work
		if (WaveActiveAllTrue(thread_read_index == ~0u))
		{
			uint base_index = 0;

			if (WaveIsFirstLane())
				InterlockedAdd(ctrl[0].read_counter, WaveGetLaneCount(), base_index);

			base_index = WaveReadLaneFirst(base_index);

			thread_read_index = base_index + WaveGetLaneIndex();
			thread_read_index = (thread_read_index >= max_queue_size) ? ~0u : thread_read_index;

			// If all read offsets are out of bounds, we are done for sure
			if (WaveActiveAllTrue(thread_read_index == ~0u))
				break;
		}

		// Attempt to fetch valid work from the current state of thread_read_index
		bool thread_runnable = false;
		TraversalItem item;

		while (true)
		{
			if (thread_read_index != ~0u)
			{
				DeviceMemoryBarrier();

				item = queue[thread_read_index];

				// Reading may be ahead of writing - value might still be cleared
				thread_runnable =
					(item.instance_id != ~0u) &&
					(item.packed != ~0u);
			}

			if (WaveActiveAnyTrue(thread_runnable))
				break;

			// Entire wave saw no valid work.
			// Check if there are actually any tasks left in-flight.
			int task_counter = 0;

			if (WaveIsFirstLane())
				InterlockedAdd(ctrl[0].task_counter, 0, task_counter);

			if (WaveReadLaneFirst(task_counter) == 0)
				return;
		}

		// Some threads have data ready to consume
		if (WaveActiveAnyTrue(thread_runnable))
		{
			int thread_sub_count = 0;

			if (thread_runnable)
				thread_sub_count = item.getSubCount();

			// Process all tasks in packed fashion across the wave.
			// We mix node/group tasks across the wave - both require
			// traversal logic operating on the same data types.
			processAllSubTasks(item, thread_sub_count);

			// All processed items decrement the global task counter
			uint num_runnable = WaveActiveCountBits(thread_runnable);

			if (WaveIsFirstLane() && num_runnable > 0)
			{
				int dummy;
				InterlockedAdd(ctrl[0].task_counter, -int(num_runnable), dummy);
			}

			// Mark as consumed to get new work next iteration
			if (thread_runnable)
				thread_read_index = ~0u;
		}
	}
}

[numthreads(THREADGROUP_SIZE, 1, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID)
{
	run();
}
