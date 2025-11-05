struct DDGIVolume
{
	float3 origin;
	float pad0;
	int3 size;
	float pad1;
	float3 spacing;
	float pad2;
	float4 sun_dir;
	float4 sun_color;
	float3 random_vector;
	float pad3;
	float random_angle;
	float use_relocation;
	float use_classification;
	uint rays_per_probe;
	uint ray_data_buffer_id;
	uint distance_atlas_tex_id;
	uint irradiance_atlas_tex_id;
	uint metadata_atlas_tex_id;
};

#define DISTANCE_TEXELS 16
#define DISTANCE_INTERIOR_TEXELS 14

#define IRRADIANCE_TEXELS 8
#define IRRADIANCE_INTERIOR_TEXELS 6

#define IRRADIANCE_ENCODE_GAMMA 5.0
#define BACKFACE_DISTANCE_SCALE 0.2

// Fixed rays are used for probe relocation/classification, for avoiding random and unstable results.
// These rays are NOT used in blending
#define USE_FIXED_RAYS 1
#define NUM_FIXED_RAYS 32

// States for probes
#define STATE_ENABLED 0 
#define STATE_DISABLED 1

#define BACKFACE_THRESHOLD_UPDATE 0.15f
#define BACKFACE_THRESHOLD_CLASSIFICATION 0.25f
#define DEFAULT_BLEND_FACTOR 0.97f
#define DISTANCE_WEIGHT_POWER 50.0f

uint GetProbeCount(DDGIVolume volume)
{
	return uint(volume.size.x) * uint(volume.size.y) * uint(volume.size.z);
}

uint GetProbesPerLayer(DDGIVolume volume)
{
	return uint(volume.size.x) * uint(volume.size.z);
}

uint GetRayDataIndex(uint probe_id, uint ray_index, DDGIVolume volume)
{
	return probe_id * volume.rays_per_probe + ray_index;
}

uint3 GetProbeCoords(DDGIVolume volume, uint probe_id)
{
	uint3 coord;
	coord.x = probe_id % uint(volume.size.x);
	coord.y = probe_id / (uint(volume.size.x) * uint(volume.size.z));
	coord.z = (probe_id / uint(volume.size.x)) % uint(volume.size.z);
	return coord;
}

uint GetProbeIndex(DDGIVolume volume, uint3 coords)
{
	return coords.x + coords.z * uint(volume.size.x) + coords.y * GetProbesPerLayer(volume);
}

uint2 GetProbeStartTexelCoords(DDGIVolume volume, uint3 probe_coords)
{
	float layer_offset = volume.size.x * probe_coords.y;
	return uint2(probe_coords.x + layer_offset, probe_coords.z);
}

float3 GetProbeWorldBasePosition(DDGIVolume volume, uint3 probe_coords)
{
	return volume.origin + probe_coords * volume.spacing;
}

float3 GetProbeRelocationOffset(DDGIVolume volume, uint3 probe_coords)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2D<float4> metadata_atlas = ResourceDescriptorHeap[volume.metadata_atlas_tex_id];
	return metadata_atlas.Load(uint3(texel_coord.xy, 0)).xyz * volume.spacing;
}

// Returns world space offset
float3 GetProbeRelocationOffset(DDGIVolume volume, uint3 probe_coords, RWTexture2D<float4> metadata_atlas)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	return metadata_atlas[texel_coord].xyz * volume.spacing;
}

uint GetProbeState(DDGIVolume volume, uint3 probe_coords)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2D<float4> metadata_atlas = ResourceDescriptorHeap[volume.metadata_atlas_tex_id];
	return metadata_atlas[texel_coord].a;
}

bool IsProbeDisabled(DDGIVolume volume, uint3 probe_coords)
{
	if (!volume.use_classification)
		return false;
	
	uint state = GetProbeState(volume, probe_coords);
	return state == STATE_DISABLED;
}

float3 GetProbeWorldPosition(DDGIVolume volume, uint3 probe_coords)
{
	float3 base_position = GetProbeWorldBasePosition(volume, probe_coords);
	if (volume.use_relocation)
	{
		float3 relocation_offset = GetProbeRelocationOffset(volume, probe_coords);
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

float3 SphericalFibonacci(float sample_index, float num_samples)
{
	const float b = (sqrt(5.f) * 0.5f + 0.5f) - 1.f;
	float phi = PI2 * frac(sample_index * b);
	float cos_theta = 1.f - (2.f * sample_index + 1.f) * (1.f / num_samples);
	float sin_theta = sqrt(saturate(1.f - (cos_theta * cos_theta)));

	return float3((cos(phi) * sin_theta), (sin(phi) * sin_theta), cos_theta);
}

float3x3 AngleAxis3x3(float angle, float3 axis)
{
	// Rotation with angle (in radians) and axis
	float c, s;
	sincos(angle, s, c);

	float t = 1.0f - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	return float3x3(
		t * x * x + c,      t * x * y - s * z,  t * x * z + s * y,
		t * x * y + s * z,  t * y * y + c,      t * y * z - s * x,
		t * x * z - s * y,  t * y * z + s * x,  t * z * z + c
	);
}

float3 GetProbeRayDirection(uint ray_index, DDGIVolume volume)
{
	bool is_fixed_ray = false;
	int ray = ray_index;
	int num_rays = volume.rays_per_probe;

	#if USE_FIXED_RAYS
		if (volume.use_relocation || volume.use_classification)
		{
			is_fixed_ray = ray_index < NUM_FIXED_RAYS;
			ray = is_fixed_ray ? ray_index : ray_index - NUM_FIXED_RAYS;
			num_rays = is_fixed_ray ? NUM_FIXED_RAYS : volume.rays_per_probe - NUM_FIXED_RAYS;
		}
	#endif

	float3 rotation = SphericalFibonacci(ray, num_rays);

	if (is_fixed_ray)
		return normalize(rotation);

	float3x3 random_rotation = AngleAxis3x3(volume.random_angle, volume.random_vector);
	return normalize(mul(rotation, random_rotation));
}

float GetVolumeWeight(float3 world_position, DDGIVolume volume)
{
	float3 size = (volume.size - 1) * volume.spacing;
	float3 volume_center = volume.origin + size * 0.5;
	float3 relative_position = world_position - volume_center;

	float3 delta = abs(relative_position) - size * 0.5f;
	if (all(delta < 0.0f))
		return 1.0f;

	float weight = 1.0f;
	weight *= 1.0f - saturate(delta.x / volume.spacing.x);
	weight *= 1.0f - saturate(delta.y / volume.spacing.y);
	weight *= 1.0f - saturate(delta.z / volume.spacing.z);
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

float3 GetSurfaceBias(float3 surface_normal, float3 camera_direction)
{
	return (surface_normal * 0.1) + (-camera_direction * 0.3);
}

float3 SampleIrradiance(float3 world_position, float3 world_normal, float3 surface_bias, DDGIVolume volume)
{
	float3 original_world_position = world_position;

	world_position += surface_bias;

	float3 sum_irradiance = 0;
	float sum_weight = 0;

	float3 relative_position = world_position - volume.origin;
	int3 base_probe_coords = int3(relative_position / volume.spacing);
	//base_probe_coords = (volume.size - 1) * 0.5;
	base_probe_coords = clamp(base_probe_coords, int3(0, 0, 0), volume.size - 1);
	
	float3 base_probe_world_position = GetProbeWorldBasePosition(volume, base_probe_coords);
	
	// Alpha is how far from the floor(currentVertex) position. on [0, 1] for each axis.
	float3 alpha = saturate((world_position - base_probe_world_position) / volume.spacing);

	for (int i = 0; i < 8; i++)
	{
		int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
		
		int3 probe_coords = clamp(base_probe_coords + offset, int3(0, 0, 0), volume.size - 1);

		if (IsProbeDisabled(volume, probe_coords))
			continue;

		float2 probe_uv_irradiance = GetProbeUV(volume, probe_coords, world_normal, IRRADIANCE_TEXELS);
		float3 biased_to_probe_direction = normalize(GetProbeWorldPosition(volume, probe_coords) - world_position);
		float2 probe_uv_distance = GetProbeUV(volume, probe_coords, -biased_to_probe_direction, DISTANCE_TEXELS);

		Texture2D irradiance_atlas = ResourceDescriptorHeap[volume.irradiance_atlas_tex_id];
		float4 irradiance = irradiance_atlas.SampleLevel(linear_clamp_sampler, probe_uv_irradiance, 0);
        // Decode the tone curve, but leave a gamma = 2 curve to approximate sRGB blending
        float exponent = IRRADIANCE_ENCODE_GAMMA * 0.5f;
        irradiance = pow(irradiance, exponent);

		float3 probe_direction = normalize(GetProbeWorldPosition(volume, probe_coords) - original_world_position);
		
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

		Texture2D distance_atlas = ResourceDescriptorHeap[volume.distance_atlas_tex_id];
		// Sample the probe's distance texture to get the mean distance to nearby surfaces
		float2 filteredDistance = 2.f * distance_atlas.SampleLevel(linear_clamp_sampler, probe_uv_distance, 0).rg;

		// Find the variance of the mean distance
		float variance = abs((filteredDistance.x * filteredDistance.x) - filteredDistance.y);

		float probe_distance = length(GetProbeWorldPosition(volume, probe_coords) - world_position);
		// Occlusion test
		float chebyshevWeight = 1.f;
		if(probe_distance > filteredDistance.x) // occluded
		{
			// v must be greater than 0, which is guaranteed by the if condition above.
			float v = probe_distance - filteredDistance.x;
			chebyshevWeight = variance / (variance + (v * v));

			// Increase the contrast in the weight
			chebyshevWeight = max((chebyshevWeight * chebyshevWeight * chebyshevWeight), 0.f);
		}

		// Avoid visibility weights ever going all the way to zero because
		// when *no* probe has visibility we need a fallback value
		weight *= max(0.05f, chebyshevWeight);

		// Avoid a weight of zero
		weight = max(0.000001f, weight);

		const float crushThreshold = 0.2f;
		if (weight < crushThreshold)
		{
			weight *= (weight * weight) * (1.f / (crushThreshold * crushThreshold));
		}

		float3 trilinear = max(0.001f, lerp(1.0 - alpha, alpha, offset));
		weight *= trilinear.x * trilinear.y * trilinear.z;

		sum_irradiance += irradiance.rgb * weight;
		sum_weight += weight;
	}

	if(sum_weight == 0.0f) return 0.0f;

	sum_irradiance *= (1.0f / sum_weight);
	sum_irradiance *= sum_irradiance;
	sum_irradiance *= PI;
	return sum_irradiance;
}