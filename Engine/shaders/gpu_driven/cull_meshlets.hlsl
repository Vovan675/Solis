#include "streaming.h"
// Reference: NVIDIA vk_lod_clusters

cbuffer Uniforms : register(b0)
{
	float4x4 frustum_view_projection;

	uint queue_buffer_id;
	uint traversal_ctrl_buffer_id;

	uint visible_meshlets_buffer_id;
	uint visible_meshlets_count_buffer_id;
	uint draw_indexed_args_buffer_id;
	uint draw_indexed_count_buffer_id;
	uint draw_calls_indirect_instances_buffer_id;
	uint max_queue_size;
	uint meshlet_visibility_buffer_id;
	uint hiz_tex_id;
	uint hiz_width;
	uint hiz_height;
	uint hiz_mips;
	uint group_residency_buffer_id;
	uint stream_requests_buffer_id;
	uint group_ages_buffer_id;
};

static globallycoherent RWStructuredBuffer<TraversalItem> queue = ResourceDescriptorHeap[queue_buffer_id];
static globallycoherent RWStructuredBuffer<TraversalCtrl> ctrl = ResourceDescriptorHeap[traversal_ctrl_buffer_id];
static RWStructuredBuffer<MeshletCandidate> visible_meshlets = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static RWStructuredBuffer<uint> visible_meshlets_count = ResourceDescriptorHeap[visible_meshlets_count_buffer_id];

#if !USE_MESH_SHADERS
	static RWStructuredBuffer<DrawIndexedIndirect> draw_args = ResourceDescriptorHeap[draw_indexed_args_buffer_id];
	static RWByteAddressBuffer draw_count = ResourceDescriptorHeap[draw_indexed_count_buffer_id];
	static RWByteAddressBuffer indirect_instances = ResourceDescriptorHeap[draw_calls_indirect_instances_buffer_id];
#endif

static RWByteAddressBuffer group_residency = ResourceDescriptorHeap[group_residency_buffer_id];
static RWByteAddressBuffer stream_requests = ResourceDescriptorHeap[stream_requests_buffer_id];
static globallycoherent RWByteAddressBuffer group_ages = ResourceDescriptorHeap[group_ages_buffer_id];
static RWByteAddressBuffer meshlet_visibility = ResourceDescriptorHeap[meshlet_visibility_buffer_id];

void resetAge(uint group_id, uint group_residency_offset)
{
	uint slot = (group_residency_offset + group_id) * 4;
	if (group_ages.Load(slot) != PINNED_GROUP_AGE)
		group_ages.Store(slot, 0);
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

	uint residency_base = mesh.group_residency_offset;
	float scale = getScaleFromTransform(instance.world_transform);

	// NODE PATH - per child BVH node: descend further, enqueue as LOD group, or fire a streaming request.
	if (!is_group)
	{
		uint child_offset = item.getNodeChildOffset();
		LodNode child_node = lod_nodes_buffer.Load<LodNode>(sizeof(LodNode) * (mesh.lod_nodes_offset + child_offset + sub_id));

		// If child too corase we must go deeper
		bool is_child_too_coarse = isCoarserThanNeeded(child_node.center, child_node.radius, child_node.error, instance.world_transform, scale);
		bool is_leaf = (child_node.child_count == 0);

		// Nodes are just BVH descriptions, no real meshlets, just for traversal
		bool descend_node = is_valid && is_child_too_coarse && !is_leaf;

		// Leaves map to real meshlet groups
		bool enqueue_group = is_valid && is_child_too_coarse && is_leaf;

		// Check if meshlet group is resident, if not dont render it and make streaming request
		if (enqueue_group)
		{
			uint residency_slot = residency_base + child_node.group_index;
			if (!isResident(group_residency, residency_base, child_node.group_index))
			{
				// Per frame deduplicate, non resident groups dont use this slot anyway
				uint previous_frame;
				group_ages.InterlockedMax(residency_slot * 4, frame, previous_frame);
				bool first_request_this_frame = (previous_frame != frame);

				// Append stream_request
				uint request_count = WaveActiveCountBits(first_request_this_frame);
				uint request_slot = 0;
				if (WaveIsFirstLane())
					stream_requests.InterlockedAdd(0, request_count, request_slot);
				request_slot = WaveReadLaneFirst(request_slot) + WavePrefixCountBits(first_request_this_frame);

				if (first_request_this_frame && request_slot < MAX_STREAMING_REQUESTS)
					stream_requests.Store(8 + request_slot * 4, residency_slot);

				enqueue_group = false;
			}
		}

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
		uint group_id = item.getGroupIndex();

		uint meshlet_id = packMeshletId(residency_base + group_id, sub_id);

		Meshlet meshlet = getMeshlet(meshlet_id, group_residency_buffer_id);

		resetAge(group_id, residency_base);

		// Frustum culling
		float3 bound_center = meshlet.center.xyz;
		float3 bound_extent = meshlet.extent.xyz;
		transformBoundBox(bound_center, bound_extent, instance.world_transform);
		FrustumCullData cull = getFrustumCullData(bound_center, bound_extent, frustum_view_projection);

		// Occlusion culling
		bool was_visible = meshlet_visibility.Load(meshlet_id * 4) != 0;

		#if FREEZE_CULLING
			bool is_meshlet_visible = was_visible;
		#else
			bool is_meshlet_visible = cull.is_visible;
			#if IS_LATE
			if (is_meshlet_visible)
			{
				Texture2D hiz_tex = ResourceDescriptorHeap[hiz_tex_id];
				is_meshlet_visible = !isHizOcclusionCulled(cull, float2(hiz_width, hiz_height), hiz_mips, hiz_tex);
			}
			#endif
			meshlet_visibility.Store(meshlet_id * 4, is_meshlet_visible ? 1u : 0u);
		#endif

		// Pick this meshlet when:
		// 1) The finer (refined) group would be wasteful - going deeper isn't worth it, cut lands here.
		// 2) The finer group doesn't exist (finest LOD) or isn't resident - force render, no alternative.
		bool has_refined = (meshlet.refined_group_id != MOST_DETAILED_CLUSTER_GROUP_ID);
		bool need_this_level = true; // If have no finer group or its not resident then render it
		if (has_refined && isResident(group_residency, residency_base, meshlet.refined_group_id))
			need_this_level = !isCoarserThanNeeded(meshlet.refined_group_id, mesh.meshlet_lod_groups_offset, instance.world_transform, scale);

		// Render cluster only when its visible for current occlusion and selected in DAG
		bool render_cluster = is_valid && is_meshlet_visible && need_this_level;
		#if IS_LATE
			render_cluster = render_cluster && !was_visible;
		#endif
		
		// Append new draw calls
		uint append_count = WaveActiveCountBits(render_cluster);
		uint append_offset = 0;

		if (WaveIsFirstLane())
			InterlockedAdd(visible_meshlets_count[0], append_count, append_offset);

		append_offset = WaveReadLaneFirst(append_offset);
		append_offset += WavePrefixCountBits(render_cluster);

		if (render_cluster)
		{
			visible_meshlets[append_offset].instance_id = item.instance_id;
			visible_meshlets[append_offset].meshlet_id = meshlet_id;

			#if !USE_MESH_SHADERS
				GroupResidency residency = group_residency.Load<GroupResidency>(sizeof(GroupResidency) * (residency_base + meshlet.group_id));

				DrawIndexedIndirect cmd;
				cmd.index_count_per_instance = meshlet.getTriangleCount() * 3;
				cmd.instance_count = 1;
				cmd.start_index_location = uint(residency.geometry_buffer_offset + meshlet.triangle_offset) / sizeof(uint);
				cmd.base_vertex_location = 0;
				cmd.start_instance_location = append_offset;
				draw_args[append_offset] = cmd;
				indirect_instances.Store(append_offset * sizeof(uint), append_offset);
				draw_count.InterlockedAdd(0, 1);
			#endif
		}
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
	int total_runs = (total_threads + lane_count - 1) / lane_count;

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
