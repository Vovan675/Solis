#include "ao_common.h"
// Reference: https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf

cbuffer Constants : register(b0)
{
	uint view_depth_tex_id;
	uint normal_tex_id;
	uint output_tex_id;
	uint sampling_noise_tex_id;
	float radius;
	float falloff;
	float final_power;
	int resolution_scale;
};

static Texture2D<float> view_depth_texture = ResourceDescriptorHeap[view_depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
static Texture2D sampling_noise = ResourceDescriptorHeap[sampling_noise_tex_id];
[[vk::image_format("r8")]]
static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];

static int2 resolution = getScaledRenderResolution(resolution_scale);

// Far samples read smaller depth mips (Reference: XeGTAO DepthMIPSamplingOffset)
static const float DEPTH_MIP_SAMPLING_OFFSET = 3.3;
static const float MAX_DEPTH_MIP = 4.0;

float getSampleHorizonCos(int2 sample_pixel, float depth_mip, float3 position, float3 view_dir, float unoccluded_horizon_cos)
{
	float sample_view_z = view_depth_texture.SampleLevel(point_clamp_sampler, (sample_pixel + 0.5) / resolution, depth_mip);
	float3 delta = getScaledPixelViewPosition(sample_pixel, sample_view_z, resolution_scale) - position;
	float sample_distance = length(delta);
	float horizon_cos = dot(delta / sample_distance, view_dir);

	// Falloff by radius distance
	float falloff_weight = saturate((radius - sample_distance) / (falloff * radius));
	return lerp(unoccluded_horizon_cos, horizon_cos, falloff_weight);
}

// h = horizon angles, n = normal angle
float integrateArcs(float2 h, float n)
{
	float2 arcs = cos(n) + 2.0 * h * sin(n) - cos(2.0 * h - n);
	return (arcs.x + arcs.y) / 4.0;
}

[numthreads(16, 16, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(resolution)))
		return;

	float view_z = view_depth_texture[pixel];
	if (view_z >= z_far)
	{
		output_texture[pixel] = 1.0;
		return;
	}

	// Everything used in view space
	float3 normal = mul((float3x3)view, unpackGBufferNormal(normal_texture[pixel].rgb));
	float3 position = getScaledPixelViewPosition(pixel, view_z, resolution_scale);
	float3 view_dir = normalize(-position);

	float radius_pixels = radius * projection[1][1] * 0.5 * resolution.y / view_z;

	float2 noise = sampleFastNoise(sampling_noise, pixel, TEMPORAL);

	float visibility = 0.0;
	[unroll]
	for (int slice = 0; slice < SLICES; slice++)
	{
		float slice_angle = PI * (slice + noise.x) / SLICES;

		// Slice plane
		float3 slice_dir = float3(cos(slice_angle), sin(slice_angle), 0.0);
		float3 slice_tangent = slice_dir - dot(slice_dir, view_dir) * view_dir;
		float3 slice_axis = normalize(cross(slice_dir, view_dir));
		float2 screen_dir = float2(slice_dir.x, -slice_dir.y);

		// Normal in the slice
		float3 projected_normal = normal - slice_axis * dot(normal, slice_axis);
		float projected_normal_length = length(projected_normal);
		float cos_normal_angle = saturate(dot(projected_normal, view_dir) / projected_normal_length);
		float normal_angle = sign(dot(slice_tangent, projected_normal)) * acos(cos_normal_angle);

		// Horizons: x = negative side, y = positive side
		float2 unoccluded_horizon_cos = float2(cos(normal_angle - PI / 2), cos(normal_angle + PI / 2));
		float2 horizon_cos = unoccluded_horizon_cos;
		[unroll]
		for (int step = 0; step < STEPS; step++)
		{
			// Quadratic offset
			float step_fraction = (step + noise.y) / STEPS;
			float offset_pixels = max(step_fraction * step_fraction * radius_pixels, step + 1.0);
			float2 offset = offset_pixels * screen_dir;
			int2 negative_sample_pixel = floor(pixel + 0.5 - offset);
			int2 positive_sample_pixel = floor(pixel + 0.5 + offset);
			float depth_mip = clamp(log2(offset_pixels) - DEPTH_MIP_SAMPLING_OFFSET, 0.0, MAX_DEPTH_MIP);

			float2 sample_horizon_cos;
			sample_horizon_cos.x = getSampleHorizonCos(negative_sample_pixel, depth_mip, position, view_dir, unoccluded_horizon_cos.x);
			sample_horizon_cos.y = getSampleHorizonCos(positive_sample_pixel, depth_mip, position, view_dir, unoccluded_horizon_cos.y);

			horizon_cos = max(horizon_cos, sample_horizon_cos);
		}

		// Clamp horizons to the hemisphere around the normal
		float2 horizon_angles = acos(clamp(horizon_cos, -1.0, 1.0));
		horizon_angles.x = normal_angle + max(-horizon_angles.x - normal_angle, -PI / 2);
		horizon_angles.y = normal_angle + min(horizon_angles.y - normal_angle, PI / 2);

		// Integrate
		visibility += projected_normal_length * integrateArcs(horizon_angles, normal_angle);
	}
	visibility /= SLICES;

	output_texture[pixel] = pow(visibility, final_power);
}
