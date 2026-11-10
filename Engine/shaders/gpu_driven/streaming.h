#pragma once
#include "../bindless.h"
#include "culling.h"

static const float error_threshold_pixels = 1.0;
// projection[1][1] = 1 / tan(fov_y / 2)
static const float error_threshold = 2.0 * error_threshold_pixels / (projection_unjittered[1][1] * render_resolution.y);

static ByteAddressBuffer lod_groups_buffer = ResourceDescriptorHeap[global_meshlets_lod_groups_buffer_id];
static ByteAddressBuffer lod_nodes_buffer  = ResourceDescriptorHeap[global_lod_nodes_buffer_id];

LODGroup getLODGroup(uint lod_groups_offset, uint local_group_id)
{
	return lod_groups_buffer.Load<LODGroup>(sizeof(LODGroup) * (lod_groups_offset + local_group_id));
}

bool isResident(ByteAddressBuffer page_table, uint page_table_offset, LODGroup group)
{
	uint first_page = page_table_offset + unpackLocationPage(group.meshlets_location);
	for (uint page = first_page; page < first_page + group.page_count; page++)
	{
		if (page_table.Load(page * 4) == MESHLET_PAGE_NOT_RESIDENT)
			return false;
	}
	return true;
}

float getError(float3 center, float radius, float error)
{
	float dist = distance(center, camera_position.xyz) - radius;
	dist = max(dist, z_near);
	return error / dist;
}

// Returns true if COARSE ENOUGH
bool isCoarserThanNeeded(float3 c, float r, float error, float4x4 world_transform, float scale)
{
	transformBoundSphere(c, r, world_transform);
	float errorMetric = getError(c, r, error * scale);

	// Error is more than threshold (so we coarser than needed)
	return errorMetric >= error_threshold;
}
