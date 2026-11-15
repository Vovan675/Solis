#include "ao_common.h"

cbuffer Constants : register(b0)
{
	uint input_tex_id;
	uint view_depth_tex_id;
	uint normal_tex_id;
	uint output_tex_id;
	int resolution_scale;
};

static Texture2D<float> input_texture = ResourceDescriptorHeap[input_tex_id];
static Texture2D<float> view_depth_texture = ResourceDescriptorHeap[view_depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
[[vk::image_format("r8")]]
static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];

static int2 resolution = getScaledRenderResolution(resolution_scale);

// LDS samples
static const int GROUP_SIZE = 16;
static const int RADIUS = 2;
static const int SHARED_SIZE = GROUP_SIZE + 2 * RADIUS;
static const int THREAD_COUNT = GROUP_SIZE * GROUP_SIZE;

groupshared float shared_ao[SHARED_SIZE][SHARED_SIZE];
groupshared float shared_view_z[SHARED_SIZE][SHARED_SIZE];
groupshared float3 shared_view_normals[SHARED_SIZE][SHARED_SIZE];

int2 sharedCoordToPixel(uint2 group_id, int2 shared_coord)
{
	return int2(group_id) * GROUP_SIZE - RADIUS + shared_coord;
}

void fillShared(uint2 group_id, int thread_index)
{
	for (int i = thread_index; i < SHARED_SIZE * SHARED_SIZE; i += THREAD_COUNT)
	{
		int2 shared_coord = int2(i % SHARED_SIZE, i / SHARED_SIZE);
		int2 sample_pixel = clamp(sharedCoordToPixel(group_id, shared_coord), 0, resolution - 1);

		shared_ao[shared_coord.y][shared_coord.x] = input_texture[sample_pixel];
		shared_view_z[shared_coord.y][shared_coord.x] = view_depth_texture[sample_pixel];
		shared_view_normals[shared_coord.y][shared_coord.x] = mul((float3x3)view, unpackGBufferNormal(normal_texture[sample_pixel].rgb));
	}
}

struct Texel
{
	float3 position;
	float3 view_normal;
	bool sky;
};

Texel loadTexel(uint2 group_id, int2 shared_coord)
{
	float view_z = shared_view_z[shared_coord.y][shared_coord.x];

	Texel texel;
	texel.position = getScaledPixelViewPosition(sharedCoordToPixel(group_id, shared_coord), view_z, resolution_scale);
	texel.view_normal = shared_view_normals[shared_coord.y][shared_coord.x];
	texel.sky = view_z >= z_far;
	return texel;
}

float getWeight(Texel center, Texel texel)
{
	float weight = 1.0;

	// Plane weight
	weight *= getPlaneWeight(center.position, center.view_normal, texel.position);

	// Normal weight
	float normal_weight = saturate(dot(center.view_normal, texel.view_normal));
	weight *= normal_weight * normal_weight;

	return weight;
}

// Separable filter using LDS
groupshared float shared_filtered_rows[SHARED_SIZE][GROUP_SIZE];

void filterHorizontal(uint2 group_id, int thread_index)
{
	for (int i = thread_index; i < SHARED_SIZE * GROUP_SIZE; i += THREAD_COUNT)
	{
		int2 filtered_coord = int2(i % GROUP_SIZE, i / GROUP_SIZE);
		int2 center_coord = filtered_coord + int2(RADIUS, 0);
		Texel center = loadTexel(group_id, center_coord);
		if (center.sky)
			continue;

		float filtered_result = 0.0;
		float filtered_weight = 0.0;
		[unroll]
		for (int x = -RADIUS; x <= RADIUS; x++)
		{
			int2 shared_coord = center_coord + int2(x, 0);
			Texel texel = loadTexel(group_id, shared_coord);
			if (texel.sky)
				continue;

			float weight = getWeight(center, texel);
			filtered_result += shared_ao[shared_coord.y][shared_coord.x] * weight;
			filtered_weight += weight;
		}
		shared_filtered_rows[filtered_coord.y][filtered_coord.x] = filtered_result / filtered_weight;
	}
}

// Vertical pass over the filtered rows
float filterVertical(uint2 group_id, uint2 group_thread_id)
{
	int2 center_coord = int2(group_thread_id) + RADIUS;
	Texel center = loadTexel(group_id, center_coord);
	if (center.sky)
		return 1.0;

	float filtered_result = 0.0;
	float filtered_weight = 0.0;
	[unroll]
	for (int y = -RADIUS; y <= RADIUS; y++)
	{
		int2 shared_coord = center_coord + int2(0, y);
		Texel texel = loadTexel(group_id, shared_coord);
		if (texel.sky)
			continue;

		float weight = getWeight(center, texel);
		filtered_result += shared_filtered_rows[shared_coord.y][group_thread_id.x] * weight;
		filtered_weight += weight;
	}
	return filtered_result / filtered_weight;
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID, uint2 group_id : SV_GroupID, uint2 group_thread_id : SV_GroupThreadID, uint thread_index : SV_GroupIndex)
{
	fillShared(group_id, thread_index);
	GroupMemoryBarrierWithGroupSync();

	filterHorizontal(group_id, thread_index);
	GroupMemoryBarrierWithGroupSync();

	if (any(pixel >= uint2(resolution)))
		return;

	output_texture[pixel] = filterVertical(group_id, group_thread_id);
}
