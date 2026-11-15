#pragma once
#include "../common.h"

int2 getScaledRenderResolution(int scale)
{
	return (int2(render_resolution.xy) + scale - 1) / scale;
}

float2 getScaledPixelUV(int2 pixel, int scale)
{
	return (pixel * scale + 0.5) * render_resolution.zw;
}

static float3 view_ray_origin = GetVSPositionFromViewZ(float2(0.0, 0.0), 1.0);
static float3 view_ray_dx = GetVSPositionFromViewZ(float2(1.0, 0.0), 1.0) - view_ray_origin;
static float3 view_ray_dy = GetVSPositionFromViewZ(float2(0.0, 1.0), 1.0) - view_ray_origin;

float3 getScaledPixelViewPosition(int2 pixel, float view_z, int scale)
{
	float2 uv = getScaledPixelUV(pixel, scale);
	return (view_ray_origin + uv.x * view_ray_dx + uv.y * view_ray_dy) * view_z;
}

// Weights of the texels at offsets (0, 0), (1, 0), (0, 1), (1, 1)
float4 getBilinearWeights(float2 fraction)
{
	return float4(
		(1 - fraction.x) * (1 - fraction.y), fraction.x * (1 - fraction.y),
		(1 - fraction.x) * fraction.y, fraction.x * fraction.y
	);
}

float getPlaneWeight(float3 position, float3 normal, float3 sample_position)
{
	float plane_distance = abs(dot(sample_position - position, normal));
	return saturate(1.0 - plane_distance / (0.01 * length(position)));
}

// FAST noise is 128x128 with flipbook on Y
float2 sampleFastNoise(Texture2D noise, uint2 pixel, bool temporal)
{
	uint2 noise_pixel = pixel & 127;
	if (temporal)
		noise_pixel.y += (frame & 31) * 128;
	return noise[noise_pixel].xy;
}
