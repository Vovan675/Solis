struct DDGICascade
{
	float4 min;
	float4 spacing;
};

struct DDGIVolume
{
	float3 origin;
	float pad0;
	int4 size; // cascade size (xyz), probes in cascade (w)
	float3 spacing;
	float pad2;
	float4 sun_dir;
	float4 sun_color;
	float3 random_vector;
	float pad3;
	float random_angle;
	float use_relocation;
	float use_classification;
	uint cascades_count;
	DDGICascade cascades[5];
	uint rays_per_probe;
	uint ray_data_buffer_id;
	uint distance_atlas_tex_id;
	uint irradiance_atlas_tex_id;
	uint metadata_atlas_tex_id;
	float3 pad4;
};

#define DISTANCE_TEXELS 12
#define DISTANCE_INTERIOR_TEXELS 10

#define IRRADIANCE_TEXELS 8
#define IRRADIANCE_INTERIOR_TEXELS 6

#define IRRADIANCE_ENCODE_GAMMA 1.5
#define BACKFACE_DISTANCE_SCALE 0.2

#define SRGB_BLENDING 1

// Fixed rays are used for probe relocation/classification, for avoiding random and unstable results.
// These rays are NOT used in blending
//#define USE_FIXED_RAYS 1
#define NUM_FIXED_RAYS 32

// States for probes
#define STATE_ENABLED 0 
#define STATE_DISABLED 1

#define BACKFACE_THRESHOLD_UPDATE 0.15f
#define BACKFACE_THRESHOLD_CLASSIFICATION 0.25f
#define DEFAULT_BLEND_FACTOR 0.97f
#define DISTANCE_WEIGHT_POWER 25.0f

uint GetProbeCount(DDGIVolume volume)
{
	return uint(volume.size.x) * uint(volume.size.y) * uint(volume.size.z) * uint(volume.cascades_count);
}

uint GetProbesPerLayer(DDGIVolume volume)
{
	return uint(volume.size.x) * uint(volume.size.z);
}

uint GetRayDataIndex(uint probe_id, uint ray_index, DDGIVolume volume)
{
	return probe_id * volume.rays_per_probe + ray_index;
}

uint GetProbeCascade(DDGIVolume volume, uint probe_id)
{
	return probe_id / volume.size.w;
}

// Returns local coords in cascade
uint3 GetProbeGridCoords(DDGIVolume volume, uint probe_id)
{
	uint local_probe_id = probe_id % volume.size.w;
	uint3 coord;
	coord.x = local_probe_id % uint(volume.size.x);
	coord.y = local_probe_id / (uint(volume.size.x) * uint(volume.size.z));
	coord.z = (local_probe_id / uint(volume.size.x)) % uint(volume.size.z);
	return coord;
}

uint GetProbeIndex(DDGIVolume volume, uint3 coords, uint cascade)
{
	uint cascade_offset = volume.size.w * cascade;
	return coords.x + coords.z * uint(volume.size.x) + coords.y * GetProbesPerLayer(volume) + cascade_offset;
}

uint2 GetProbeStartTexelCoords(DDGIVolume volume, uint3 probe_coords)
{
	float layer_offset = volume.size.x * probe_coords.y;
	return uint2(probe_coords.x + layer_offset, probe_coords.z);
}

float3 GetProbeWorldBasePosition(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	DDGICascade casc = volume.cascades[cascade];
	return casc.min.xyz + probe_coords * casc.spacing.xyz;
}

float3 GetProbeRelocationOffset(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[volume.metadata_atlas_tex_id];
	DDGICascade casc = volume.cascades[cascade];
	return metadata_atlas.Load(uint4(texel_coord.xy, cascade, 0)).xyz * casc.spacing.xyz;
}

// Returns world space offset
float3 GetProbeRelocationOffset(DDGIVolume volume, uint3 probe_coords, uint cascade, RWTexture2DArray<float4> metadata_atlas)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	DDGICascade casc = volume.cascades[cascade];
	return metadata_atlas[uint3(texel_coord, cascade)].xyz * casc.spacing.xyz;
}

uint GetProbeState(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[volume.metadata_atlas_tex_id];
	return metadata_atlas.Load(uint4(texel_coord.xy, cascade, 0)).a;
}

bool IsProbeDisabled(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	if (!volume.use_classification)
		return false;
	
	uint state = GetProbeState(volume, probe_coords, cascade);
	return state == STATE_DISABLED;
}

float3 GetProbeWorldPosition(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	float3 base_position = GetProbeWorldBasePosition(volume, probe_coords, cascade);
	if (volume.use_relocation)
	{
		float3 relocation_offset = GetProbeRelocationOffset(volume, probe_coords, cascade);
		base_position += relocation_offset;
	}
	return base_position;
}

float3 DecodeNormalOctahedron(float2 f)
{
	float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += lerp(t, -t, n.xy >= 0.0);
	return normalize(n);
}

float2 GetNormalizedOctahedralCoordinates(int2 tex_coords, int num_texels)
{
	// Map 2D texture coordinates to a normalized octahedral space
	float2 oct_coord = float2(tex_coords.x % num_texels, tex_coords.y % num_texels);

	// Move to the center of a texel
	oct_coord.xy += 0.5f;

	// Normalize
	oct_coord.xy /= float(num_texels);

	// Shift to [-1, 1)
	oct_coord *= 2.f;
	oct_coord -= float2(1.f, 1.f);

	return oct_coord;
}

float SignNotZero(float v)
{
	return (v >= 0.f) ? 1.f : -1.f;
}
float2 SignNotZero(float2 v)
{
	return float2(SignNotZero(v.x), SignNotZero(v.y));
}

// Compute the normalized octahedral direction from [-1, 1] square
float3 GetOctahedralDirection(float2 coords)
{
	float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));
	if (direction.z < 0.f)
	{
		direction.xy = (1.f - abs(direction.yx)) * SignNotZero(direction.xy);
	}
	return normalize(direction);
}


// Compute the octant coordinates in the normalized [-1, 1] square, for unit direction vector.
float2 GetOctahedralCoordinates(float3 direction)
{
	float l1_norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
	float2 uv = direction.xy * (1.f / l1_norm);
	if (direction.z < 0.f)
	{
		uv = (1.f - abs(uv.yx)) * SignNotZero(uv.xy);
	}
	return uv;
}

// Tracing result is 20x20 pixels
#define TRACE_TEXELS 20

float3 GetProbeRayDirection(uint ray_index, DDGIVolume volume)
{
	uint2 ray_texel = uint2(ray_index % TRACE_TEXELS, ray_index / TRACE_TEXELS);

	float2 oct_coord = GetNormalizedOctahedralCoordinates(ray_texel.xy, TRACE_TEXELS);
	float3 texel_direction = GetOctahedralDirection(oct_coord);
	return normalize(texel_direction);
}

float GetVolumeWeight(float3 world_position, DDGIVolume volume)
{
	DDGICascade last_cascade = volume.cascades[volume.cascades_count - 1];
	float3 size = (volume.size.xyz - 1) * last_cascade.spacing.xyz;
	float3 volume_center = last_cascade.min.xyz + size * 0.5;
	float3 relative_position = world_position - volume_center;

	float3 delta = abs(relative_position) - size * 0.5f;
	if (all(delta < 0.0f))
		return 1.0f;

	float weight = 1.0f;
	weight *= 1.0f - saturate(delta.x / last_cascade.spacing.x);
	weight *= 1.0f - saturate(delta.y / last_cascade.spacing.y);
	weight *= 1.0f - saturate(delta.z / last_cascade.spacing.z);
	return weight;
}

float2 GetProbeUV(DDGIVolume volume, uint3 probe_coords, float3 direction, int texels)
{
	float texture_width = (volume.size.x * texels) * volume.size.y;
	float texture_height = volume.size.z * texels;
	float layer_offset = volume.size.x * texels * probe_coords.y;

	float2 start_uv = float2(probe_coords.x * texels + layer_offset, probe_coords.z * texels);
	start_uv += 1;
	//start_uv += texels / 2.0f; // just for test
	float2 oct_coord = (GetOctahedralCoordinates(direction) * 0.5 + 0.5) * (texels - 2);
	start_uv += oct_coord;

	start_uv /= float2(texture_width, texture_height);
	return start_uv;
}

bool GetCascadeForPosition(DDGIVolume volume, out DDGICascade cascade, out uint cascade_index, float3 world_pos)
{
	// Find minimum cascade where position is inside
	for (int i = 0; i < volume.cascades_count; i++)
	{
		cascade = volume.cascades[i];
		float3 cascade_max = cascade.min.xyz + volume.size.xyz * cascade.spacing.xyz;
		if (all(world_pos > cascade.min.xyz) && all(world_pos < cascade_max))
		{
			cascade_index = i;
			return true;
		}
	}

	// If no cascade found return false and last cascade
	cascade_index = volume.cascades_count - 1;
	cascade = volume.cascades[cascade_index];
	return false;
}

float3 GetSurfaceBias(float3 surface_normal, float3 camera_position, float3 sample_position, DDGIVolume volume, uint cascade_id)
{
	DDGICascade cascade = volume.cascades[cascade_id];
	float BIAS_FACTOR = 0.25f;
    float NORMAL_TO_VIEW_WEIGHT = 0.3f;
    float origin_to_sample_dst = length(camera_position - sample_position);
	float spacing = max(cascade.spacing.x, max(cascade.spacing.y, cascade.spacing.z));
    float sample_offset = min(spacing * BIAS_FACTOR, origin_to_sample_dst * 0.5f);
    float3 sample_to_origin = normalize(camera_position - sample_position);
    return lerp(sample_to_origin, surface_normal, NORMAL_TO_VIEW_WEIGHT) * sample_offset;
	//return (surface_normal * 0.1) + (-camera_direction * 0.3);
}

float3 SampleIrradiance(float3 world_position, float3 world_normal, float3 surface_bias, DDGIVolume volume, uint cascade_id)
{
	DDGICascade cascade = volume.cascades[cascade_id];
	float3 original_world_position = world_position;

	world_position += surface_bias;

	float3 sum_irradiance = 0;
	float sum_weight = 0;

	float3 relative_position = world_position - cascade.min.xyz;
	int3 base_probe_coords = int3(relative_position / cascade.spacing.xyz);
	//base_probe_coords = (volume.size - 1) * 0.5;
	base_probe_coords = clamp(base_probe_coords, int3(0, 0, 0), volume.size.xyz - 1);
	
	float3 base_probe_world_position = GetProbeWorldBasePosition(volume, base_probe_coords, cascade_id);
	// Alpha is how far from the floor(currentVertex) position. on [0, 1] for each axis.
	float3 alpha = saturate((world_position - base_probe_world_position) / cascade.spacing.xyz);

	for (int i = 0; i < 8; i++)
	{
		int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
		
		int3 probe_coords = clamp(base_probe_coords + offset, int3(0, 0, 0), volume.size.xyz - 1);

		if (IsProbeDisabled(volume, probe_coords, cascade_id))
			continue;

		float2 probe_uv_irradiance = GetProbeUV(volume, probe_coords, world_normal, IRRADIANCE_TEXELS);
		float3 biased_to_probe = GetProbeWorldPosition(volume, probe_coords, cascade_id) - world_position;
		float3 biased_to_probe_direction = normalize(biased_to_probe);
		float2 probe_uv_distance = GetProbeUV(volume, probe_coords, -biased_to_probe_direction, DISTANCE_TEXELS);

		Texture2DArray irradiance_atlas = ResourceDescriptorHeap[volume.irradiance_atlas_tex_id];
		float4 irradiance = irradiance_atlas.SampleLevel(linear_clamp_sampler, float3(probe_uv_irradiance, cascade_id), 0);

		#if SRGB_BLENDING
			// Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending
			float exponent = IRRADIANCE_ENCODE_GAMMA * 0.5f;
			irradiance = pow(irradiance, exponent);
		#endif

		float3 probe_direction = normalize(GetProbeWorldPosition(volume, probe_coords, cascade_id) - original_world_position);
		
		#if 0
			// This creates a sharp cutoff at the edge, visible when classification is used and probes are disabled
			float weight = saturate(dot(probe_direction, world_normal));
		#elif 0
			// [Sloan et al. 11] “Wrap Shading”
			// Smooth transition (like NVIDIA DDGI)
			float wrapShading = (dot(probe_direction, world_normal) + 1.0f) * 0.5f;
			float weight = (wrapShading * wrapShading) + 0.2f;
		#else
			// Another way of smooth transition
			float wrapShading = (dot(probe_direction, world_normal) + 1.0f) * 0.5f;
			float weight = (wrapShading * wrapShading);
		#endif

		Texture2DArray distance_atlas = ResourceDescriptorHeap[volume.distance_atlas_tex_id];

		// Sample the probe's distance texture to get the mean distance to nearby surfaces
		float2 filtered_distance = distance_atlas.SampleLevel(linear_clamp_sampler, float3(probe_uv_distance, cascade_id), 0).rg;

		// Find the variance of the mean distance
		float variance = abs((filtered_distance.x * filtered_distance.x) - filtered_distance.y);

		float biased_to_probe_distance = length(biased_to_probe);
		// Occlusion test
		float chebyshev_weight = 1.f;
		if(biased_to_probe_distance > filtered_distance.x) // occluded
		{
			// v must be greater than 0, which is guaranteed by the if condition above.
			float v = biased_to_probe_distance - filtered_distance.x;
			chebyshev_weight = variance / (variance + (v * v));

			// Increase the contrast in the weight
			chebyshev_weight = max((chebyshev_weight * chebyshev_weight * chebyshev_weight), 0.f);
		}

		// Avoid visibility weights ever going all the way to zero because
		// when *no* probe has visibility we need a fallback value
		weight *= max(0.05f, chebyshev_weight);

		// Avoid a weight of zero
		weight = max(1e-6f, weight);

		// Crush tiny weights
		const float crush_threshold = 0.2f;
		if (weight < crush_threshold)
		{
			weight *= (weight * weight) * (1.f / (crush_threshold * crush_threshold));
		}

		float3 trilinear = max(0.001f, lerp(1.0 - alpha, alpha, offset));
		weight *= trilinear.x * trilinear.y * trilinear.z;

		sum_irradiance += irradiance.rgb * weight;
		sum_weight += weight;
	}

	if(sum_weight == 0.0f) return 0.0f;

	sum_irradiance /= sum_weight;
	#if SRGB_BLENDING
		sum_irradiance *= sum_irradiance;
	#endif
	sum_irradiance *= PI2;  // Multiply by the area of the integration domain (hemisphere) for Monte Carlo
	return sum_irradiance;
}