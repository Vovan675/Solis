#include "common.h"

#define OUTLINE_FILL 0.1
#define OUTLINE_THICKNESS 2

static const float3 OUTLINE_COLOR = float3(0.92, 0.63, 0.05);

static const int2 NEIGHBOURS[8] =
{
	int2(-1, -1), int2(0, -1), int2(1, -1),
	int2(-1, 0), int2(1, 0),
	int2(-1, 1), int2(0, 1), int2(1, 1)
};

cbuffer Constants : register(b0)
{
	uint outline_depth_tex_id;
	uint scene_depth_tex_id;
};

static Texture2D outline_depth_texture = ResourceDescriptorHeap[outline_depth_tex_id];
static Texture2D scene_depth_texture = ResourceDescriptorHeap[scene_depth_tex_id];

struct VSInput
{
	float2 uv : TEXCOORD0;
	float4 position : SV_POSITION;
};

bool isVisible(int2 pixel, float depth)
{
	int2 scene_pixel = int2((pixel + 0.5) * render_resolution.xy * output_resolution.zw);
	float scene_depth = scene_depth_texture.Load(int3(scene_pixel, 0)).r;
	return depth >= scene_depth * 0.99;
}

float getOccludedOpacity(uint2 pixel)
{
	return ((pixel.x >> 1) + (pixel.y >> 1)) % 2 == 0 ? 0.2 : 0.7;
}

float4 PSMain(VSInput input) : SV_Target
{
	int2 pixel = int2(input.position.xy);
	
	float depth = outline_depth_texture.Load(int3(pixel, 0)).r;
	bool inside = depth > 0.0;
	bool visible = inside && isVisible(pixel, depth);

	bool border = false;
	bool border_occluded = visible;
	for (int i = 0; i < 8; i++)
	{
		int2 neighbour = clamp(pixel + NEIGHBOURS[i] * OUTLINE_THICKNESS, 0, int2(output_resolution.xy) - 1);
		float neighbour_depth = outline_depth_texture.Load(int3(neighbour, 0)).r;
		bool neighbour_inside = neighbour_depth > 0.0;
		if (neighbour_inside == inside)
			continue;

		border = true;
		if (neighbour_inside && isVisible(neighbour, neighbour_depth))
			border_occluded = true;
	}

	float opacity = 0.0;
	if (border)
		opacity = border_occluded ? 1.0 : getOccludedOpacity(pixel);
	else if (inside)
		opacity = OUTLINE_FILL * (visible ? 1.0 : getOccludedOpacity(pixel));
	return float4(OUTLINE_COLOR, opacity);
}
