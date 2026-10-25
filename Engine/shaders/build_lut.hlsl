#include "common.h"
#include "tonemap.h"

cbuffer Constants : register(b0)
{
	uint output_tex_id;
	int tonemapper_mode;
};

[[vk::image_format("rgba16f")]]
static RWTexture3D<float4> output_texture = ResourceDescriptorHeap[output_tex_id];

[numthreads(4, 4, 4)]
void CSMain(uint3 texel : SV_DispatchThreadID)
{
	float3 color = logToLinear(texel / (TONEMAP_LUT_SIZE - 1.0));

	// Color grading
	// ...

	// Apply tone mapping
	if (tonemapper_mode == 1)
		color = Uncharted2(color) / Uncharted2(UNCHARTED2_WHITE);
	else if (tonemapper_mode == 2)
		color = ACESFitted(color);

	// Convert to sRGB
	float3 result = LinearToSRGB(color);

	output_texture[texel] = float4(result, 1.0);
}
