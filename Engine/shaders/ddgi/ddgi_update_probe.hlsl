#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
static DDGIVolume volume = volumes[0];

cbuffer Constants : register(b1)
{
	uint output_atlas_tex_id;
};

#ifdef IRRADIANCE
	[[vk::image_format("rgba16f")]]
	static RWTexture2D<float4> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];
#else
	[[vk::image_format("rg16f")]]
	static RWTexture2D<float2> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];
#endif

struct CSInput
{
	uint3 dispatch_thread_id : SV_DispatchThreadID;
	uint3 group_id : SV_GroupID;
	uint3 group_thread_id : SV_GroupThreadID;
	uint group_index : SV_GroupIndex;
};

// Copy border texels for correctly working hardware bilinear filtering (wrapping)
void WriteOctahedronBorder(uint2 texel_coord, uint2 oct_texel)
{
	uint2 source_coord = texel_coord.xy;

	bool is_left_or_right = oct_texel.x == 0 || oct_texel.x == NUM_TEXELS - 1;
	bool is_top_or_bottom = oct_texel.y == 0 || oct_texel.y == NUM_TEXELS - 1;
	if (is_left_or_right && is_top_or_bottom)
	{
		source_coord.x += oct_texel.x == 0 ? NUM_TEXELS - 1 : -NUM_TEXELS + 1;
		source_coord.y += oct_texel.y == 0 ? NUM_TEXELS - 1 : -NUM_TEXELS + 1;
	} else if (is_left_or_right)
	{
		source_coord.x += oct_texel.x == 0 ? 1 : -1;
		source_coord.y += -oct_texel.y;
		source_coord.y += NUM_TEXELS - 1 - oct_texel.y;
	} else if (is_top_or_bottom)
	{
		source_coord.y += oct_texel.y == 0 ? 1 : -1;
		source_coord.x += -oct_texel.x;
		source_coord.x += NUM_TEXELS - 1 - oct_texel.x;
	}

	output_atlas[texel_coord.xy] = output_atlas[source_coord.xy];
}

// xzy order (xz plane, y is layer)
[numthreads(NUM_TEXELS, NUM_TEXELS, 1)]
void CSMain(CSInput input)
{
	uint3 probe_coord = input.group_id.xzy;
	uint probe_id = GetProbeIndex(volume, probe_coord);

	uint layer = probe_coord.y;
	uint2 layer_offset = uint2(layer * NUM_TEXELS * uint(volume.size.x), 0);
	uint2 texel_coord = input.dispatch_thread_id.xy + layer_offset;
	
	float2 oct_coord = GetNormalizedOctahedralCoordinates(texel_coord.xy, NUM_TEXELS);
	float3 texel_direction = GetOctahedralDirection(oct_coord);

	if (IsProbeDisabled(volume, probe_coord))
		return;

	uint ray_index = 0;
	#if USE_FIXED_RAYS
		if (volume.use_relocation || volume.use_classification)
			ray_index = NUM_FIXED_RAYS;
	#endif

	#ifdef IRRADIANCE
		float3 irradiance_average = float3(0, 0, 0);
		float weight_sum = 0;

		int max_back_faces = int(float(volume.rays_per_probe) * BACKFACE_THRESHOLD_UPDATE);
		int back_face_count = 0;
		
		StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[volume.ray_data_buffer_id];
		for (; ray_index < volume.rays_per_probe; ray_index++)
		{
			uint ray_data_index = GetRayDataIndex(probe_id, ray_index, volume);
			float4 ray_data = ray_data_buffer[ray_data_index];

			float3 radiance = ray_data.rgb;
			float distance = ray_data.a;

			if (distance < 0)
			{
				back_face_count++;
				if (back_face_count >= max_back_faces)
					return;
				continue;
			}
			float3 ray_direction = GetProbeRayDirection(ray_index, volume);
			float weight = saturate(dot(ray_direction, texel_direction));

			irradiance_average += ray_data.rgb * weight;
			weight_sum += weight;
		}

		irradiance_average /= max(weight_sum, 1e-9f);
		irradiance_average = pow(irradiance_average, 1.0 / IRRADIANCE_ENCODE_GAMMA);

		float3 prev = output_atlas[texel_coord.xy].rgb;
		float blend_factor = DEFAULT_BLEND_FACTOR;
		
		// If was cleared, override previous value
		if (dot(prev, prev) == 0) blend_factor = 0;

		float blend_weight = saturate(1.0f - blend_factor);

		float3 result = lerp(prev, irradiance_average, blend_weight);
		output_atlas[texel_coord.xy] = float4(result, 1);
	#else
		float2 distance_average = float2(0, 0);
		float weight_sum = 0;

		StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[volume.ray_data_buffer_id];
		for (; ray_index < volume.rays_per_probe; ray_index++)
		{
			uint ray_data_index = GetRayDataIndex(probe_id, ray_index, volume);
			float4 ray_data = ray_data_buffer[ray_data_index];

			float3 ray_direction = GetProbeRayDirection(ray_index, volume);
			float weight = saturate(dot(ray_direction, texel_direction));
			weight = pow(weight, DISTANCE_WEIGHT_POWER);
			
			distance_average += float2(abs(ray_data.a), ray_data.a * ray_data.a) * weight;
			weight_sum += weight;
		}

		distance_average /= weight_sum;

		float blend_factor = DEFAULT_BLEND_FACTOR;
		float blend_weight = saturate(1.0f - blend_factor);

		float2 prev = output_atlas[texel_coord.xy];
		float2 result = lerp(prev, distance_average, blend_weight);
		output_atlas[texel_coord.xy] = result;
	#endif

    AllMemoryBarrierWithGroupSync();
	WriteOctahedronBorder(texel_coord, input.group_thread_id.xy);
}
