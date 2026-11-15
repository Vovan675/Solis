#include "ao_common.h"

cbuffer Constants : register(b0)
{
	uint input_tex_id;
	uint input_view_depth_tex_id;
	uint depth_tex_id;
	uint normal_tex_id;
	uint output_tex_id;
	int resolution_scale;
};

static Texture2D<float> input_texture = ResourceDescriptorHeap[input_tex_id];
static Texture2D<float> input_view_depth_texture = ResourceDescriptorHeap[input_view_depth_tex_id];
static Texture2D<float> depth_texture = ResourceDescriptorHeap[depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
[[vk::image_format("r8")]]
static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];

static int2 input_resolution = getScaledRenderResolution(resolution_scale);

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	float depth = depth_texture[pixel];
	if (depth == 0.0)
	{
		output_texture[pixel] = 1.0;
		return;
	}

	float3 normal = mul((float3x3)view, unpackGBufferNormal(normal_texture[pixel].rgb));
	float3 position = GetVSPosition((pixel + 0.5) * render_resolution.zw, depth);

	float2 input_pixel = float2(pixel) / resolution_scale;
	int2 base_pixel = floor(input_pixel);
	float4 bilinear_weights = getBilinearWeights(frac(input_pixel));

	float upsampled_result = 0.0;
	float upsampled_weight = 0.0;
	[unroll]
	for (int i = 0; i < 4; i++)
	{
		// offsets (0, 0), (1, 0), (0, 1), (1, 1)
		int2 sample_pixel = min(base_pixel + int2(i & 1, i >> 1), input_resolution - 1);
		float sample_view_z = input_view_depth_texture[sample_pixel];
		float3 sample_position = getScaledPixelViewPosition(sample_pixel, sample_view_z, resolution_scale);

		// Plane weight
		float plane_weight = getPlaneWeight(position, normal, sample_position);

		float weight = bilinear_weights[i] * max(plane_weight, 0.001);

		upsampled_result += input_texture[sample_pixel] * weight;
		upsampled_weight += weight;
	}

	output_texture[pixel] = upsampled_result / upsampled_weight;
}
