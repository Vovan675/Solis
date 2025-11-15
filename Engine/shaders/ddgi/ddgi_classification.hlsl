#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
static DDGIVolume volume = volumes[0];

cbuffer Constants : register(b1)
{
	uint output_atlas_tex_id;
};

[[vk::image_format("rgba16f")]]
static RWTexture2D<float4> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];

struct CSInput
{
	uint3 dispatch_thread_id : SV_DispatchThreadID;
	uint3 group_id : SV_GroupID;
	uint3 group_thread_id : SV_GroupThreadID;
	uint group_index : SV_GroupIndex;
};

[numthreads(32, 1, 1)]
void CS_ResetClassification(CSInput input)
{
	uint probe_id = input.dispatch_thread_id.x;
	uint num_probes = GetProbeCount(volume);

	if (probe_id >= num_probes) return;

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coord);
	output_atlas[texel_coord.xy].a = STATE_ENABLED;
}

[numthreads(32, 1, 1)]
void CS_Classification(CSInput input)
{
	uint probe_id = input.dispatch_thread_id.x;
	uint num_probes = GetProbeCount(volume);

	if (probe_id >= num_probes) return;

	int backface_count = 0;
	uint rays_count = volume.rays_per_probe;
	#if USE_FIXED_RAYS
		rays_count = NUM_FIXED_RAYS;
	#endif

	StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[volume.ray_data_buffer_id];
	for (uint ray_index = 0; ray_index < rays_count; ray_index++)
	{
		uint ray_data_index = GetRayDataIndex(probe_id, ray_index, volume);
		float ray_distance = ray_data_buffer[ray_data_index].a;

		if (ray_distance < 0)
		{
			backface_count++;
		}
	}

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coord);

	// Disable probes if they have more than % backfaces
	if ((float(backface_count) / float(rays_count)) > BACKFACE_THRESHOLD_CLASSIFICATION)
	{
		output_atlas[texel_coord.xy].a = STATE_DISABLED;
		return;
	}

	float3 probe_world_position = GetProbeWorldPosition(volume, probe_coord);

	// Enable probe if there is any nearby geometry in its voxel
	for (uint ray_index = 0; ray_index < rays_count; ray_index++)
	{
		uint ray_data_index = GetRayDataIndex(probe_id, ray_index, volume);
		float ray_distance = ray_data_buffer[ray_data_index].a;

		// Skip backfaces
		if (ray_distance < 0)
			continue;

		float3 ray_direction = GetProbeRayDirection(ray_index, volume);
	
		// Plane normals
		float3 x_normal = float3(ray_direction.x / max(abs(ray_direction.x), 0.000001f), 0, 0);
		float3 y_normal = float3(0, ray_direction.y / max(abs(ray_direction.y), 0.000001f), 0);
		float3 z_normal = float3(0, 0, ray_direction.z / max(abs(ray_direction.z), 0.000001f));

		float3 x_plane = probe_world_position + x_normal * volume.spacing.x;
		float3 y_plane = probe_world_position + y_normal * volume.spacing.y;
		float3 z_plane = probe_world_position + z_normal * volume.spacing.z;

		float3 distances =
		{
			dot((x_plane - probe_world_position), x_normal) / max(dot(ray_direction, x_normal), 0.000001f),
			dot((y_plane - probe_world_position), y_normal) / max(dot(ray_direction, y_normal), 0.000001f),
			dot((z_plane - probe_world_position), z_normal) / max(dot(ray_direction, z_normal), 0.000001f),
		};
		
		if (distances.x == 0) distances.x = 1000000.0;
		if (distances.y == 0) distances.y = 1000000.0;
		if (distances.z == 0) distances.z = 1000000.0;

		float max_distance = min(distances.x, min(distances.y, distances.z));

		if (ray_distance <= max_distance)
		{
			output_atlas[texel_coord.xy].a = STATE_ENABLED;
			return;
		}
	}

	output_atlas[texel_coord.xy].a = STATE_DISABLED;
}
