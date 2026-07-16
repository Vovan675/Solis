#pragma once
#include "../bindless.h"
#include "culling.h"

#define PINNED_GROUP_AGE 0xFFFFFFFFu

static const float camera_fov = PI / 4.0f; // TODO: compute on cpu (45 degrees)
static const float error_threshold_pixels = 1.0;
static const float error_threshold = (tan(camera_fov * 0.5) * error_threshold_pixels / render_resolution.y);

static ByteAddressBuffer lod_groups_buffer = ResourceDescriptorHeap[global_meshlets_lod_groups_buffer_id];
static ByteAddressBuffer lod_nodes_buffer  = ResourceDescriptorHeap[global_lod_nodes_buffer_id];

void requestGroup(RWByteAddressBuffer stream_requests, RWByteAddressBuffer group_ages, uint group_residency_offset, uint local_group_id)
{
	// Deduplicate
	uint flat_slot = group_residency_offset + local_group_id;
	uint old_frame;
	group_ages.InterlockedMax(flat_slot * 4, frame, old_frame);
	if (old_frame == frame) return;

	uint slot;
	stream_requests.InterlockedAdd(0, 1, slot);
	if (slot < MAX_STREAMING_REQUESTS)
		stream_requests.Store(8 + slot * 4, flat_slot);
}

bool isResident(RWByteAddressBuffer group_residency, uint group_residency_offset, uint local_group_id)
{
	GroupResidency residency = group_residency.Load<GroupResidency>(sizeof(GroupResidency) * (group_residency_offset + local_group_id));
	return residency.geometry_buffer_offset<GROUP_NON_RESIDENT_ADDRESS_START;
}

float getError(float3 center, float radius, float error)
{
	float dist = distance(center, camera_position.xyz) - radius;
	dist = max(dist, z_near);
	return error / dist;
}

// Load a LOD group by local index, transform its bounds, and return screen-space error.
float getLODGroupError(uint local_group_id, uint lod_groups_offset, float4x4 world_transform, float scale)
{
	LODGroup g = lod_groups_buffer.Load<LODGroup>(sizeof(LODGroup) * (local_group_id + lod_groups_offset));
	float3 c = g.center;
	float r = g.radius;
	transformBoundSphere(c, r, world_transform);
	return getError(c, r, g.error * scale);
}

// Returns true if COARSE ENOUGH
bool isCoarserThanNeeded(float3 c, float r, float error, float4x4 world_transform, float scale)
{
	transformBoundSphere(c, r, world_transform);
	float errorMetric = getError(c, r, error * scale);

	// Error is more than threshold (so we coarser than needed)
	return errorMetric >= error_threshold;
}

// Returns true if COARSE ENOUGH
bool isCoarserThanNeeded(uint local_group_id, uint lod_groups_offset, float4x4 world_transform, float scale)
{
	LODGroup g = lod_groups_buffer.Load<LODGroup>(sizeof(LODGroup) * (local_group_id + lod_groups_offset));
	float3 c = g.center;
	float r = g.radius;
	return isCoarserThanNeeded(c, r, g.error, world_transform, scale);
}