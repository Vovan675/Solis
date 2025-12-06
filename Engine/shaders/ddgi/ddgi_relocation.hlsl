#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
static DDGIVolume volume = volumes[0];
static StructuredBuffer<uint> probes_to_update = ResourceDescriptorHeap[volume.probes_to_update_buffer_ids];

cbuffer Constants : register(b1)
{
	uint output_atlas_tex_id;
};

[[vk::image_format("rgba16f")]]
static RWTexture2DArray<float4> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];

struct CSInput
{
	uint3 dispatch_thread_id : SV_DispatchThreadID;
	uint3 group_id : SV_GroupID;
	uint3 group_thread_id : SV_GroupThreadID;
	uint group_index : SV_GroupIndex;
};

[numthreads(32, 1, 1)]
void CS_ResetRelocation(CSInput input)
{
	uint probe_id = input.dispatch_thread_id.x;
	uint cascade_id = GetProbeCascade(volume, probe_id);

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coord);
	output_atlas[uint3(texel_coord.xy, cascade_id)].xyz = float3(0, 0, 0);
}

[numthreads(32, 1, 1)]
void CS_Relocate(CSInput input)
{
	uint probe_id = probes_to_update[input.dispatch_thread_id.x];
	uint cascade_id = GetProbeCascade(volume, probe_id);

	DDGICascade cascade = volume.cascades[cascade_id];

	int backface_count = 0;
	int closest_back_face_index = -1;
	int closest_front_face_index = -1;
	int farthest_front_face_index = -1;
	float closest_back_face_distance = 1e27f;
	float closest_front_face_distance = 1e27f;
	float farthest_front_face_distance = 0.0;

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

			// Invert distance to get the actual distance to the backface. And scale it to the actual distance.
			ray_distance = -ray_distance / BACKFACE_DISTANCE_SCALE;

			if (ray_distance < closest_back_face_distance)
			{
				closest_back_face_distance = ray_distance;
				closest_back_face_index = ray_index;
			}
		} else
		{
			if (ray_distance < closest_front_face_distance)
			{
				closest_front_face_distance = ray_distance;
				closest_front_face_index = ray_index;
			} else if(ray_distance > farthest_front_face_distance)
			{
				farthest_front_face_distance = ray_distance;
				farthest_front_face_index = ray_index;
			}
		}
	}


	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint3 texel_coord = uint3(GetProbeStartTexelCoords(volume, probe_coord), cascade_id);
	float3 offset = GetProbeRelocationOffset(volume, probe_coord, cascade_id, output_atlas);
	uint prev_state = output_atlas[texel_coord].w;
	prev_state = STATE_ENABLED;

	float minimum_front_face_distance = 0.35;
	float maximum_possible_offset = 0.45 * max(max(cascade.spacing.x, cascade.spacing.y), cascade.spacing.z);

	float3 new_offset = 1000;
	// If there's a close backface AND you see more than % backfaces, assume you're inside something.
	bool is_too_much_backfaces = (float(backface_count) / float(rays_count)) > BACKFACE_THRESHOLD_CLASSIFICATION;
	if (closest_back_face_index != -1 && is_too_much_backfaces) {
		float3 closest_back_face_direction = normalize(GetProbeRayDirection(closest_back_face_index, volume));
		
		float move_distance = closest_back_face_distance + minimum_front_face_distance * 0.1;
		new_offset = offset + closest_back_face_direction * move_distance;
	} else if (closest_front_face_index == -1 && closest_back_face_distance > maximum_possible_offset)
	{
		// If no front face visible and closest back face is too far, then its impossible to get out of surface
		// So, just disable probe
		//new_offset = float3(0, 0, 0);
	} else if (closest_front_face_distance < minimum_front_face_distance)
	{
		// If no backface, and closest front face is close, move towards the farthest front face
		float3 closest_front_face_direction = GetProbeRayDirection(closest_front_face_index, volume);
		float3 farthest_front_face_direction = GetProbeRayDirection(farthest_front_face_index, volume);

		if (dot(closest_front_face_direction, farthest_front_face_direction) <= 0.0f)
		{
			// If closest and farthest are not in the same direction, move towards farthest
			float move_distance = min(1.0, farthest_front_face_distance);
			new_offset = offset + farthest_front_face_direction * move_distance;
		}
	} else if (closest_front_face_distance > minimum_front_face_distance)
	{
		// If no backface, and closest front face is far enough, move towards zero offset
		float move_distance = min(length(offset), closest_front_face_distance - minimum_front_face_distance);
		float3 move_back_direction = normalize(-offset);
		new_offset = offset + move_back_direction * move_distance;
	}


	// Maximum offset is 45% of the spacing
	float max_offset = 0.45;
	float3 new_offset_norm = new_offset / cascade.spacing.xyz;
	if (length(new_offset_norm) < max_offset)
	{
		offset = new_offset;
	}
	
	if (length(offset) == 0 && is_too_much_backfaces)
	{
		prev_state = STATE_DISABLED;
	}

	output_atlas[texel_coord].xyz = float3(offset / cascade.spacing.xyz);
	output_atlas[texel_coord].w = prev_state;
}
