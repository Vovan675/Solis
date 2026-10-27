#include "common.h"
#include "tonemap.h"

#define USE_AP1 1

struct UchimuraParameters
{
	float max_brightness;
	float contrast;
	float linear_section_start;
	float linear_section_length;
	float black_tightness;
	float pedestal;
};

cbuffer Constants : register(b0)
{
	uint output_tex_id;
	int tonemapper_mode;
	UchimuraParameters uchimura;
};

[[vk::image_format("rgba16f")]]
static RWTexture3D<float4> output_texture = ResourceDescriptorHeap[output_tex_id];

[numthreads(4, 4, 4)]
void CSMain(uint3 texel : SV_DispatchThreadID)
{
	float3 color = logToLinear(texel / (TONEMAP_LUT_SIZE - 1.0));

	#if USE_AP1
		color = mul(REC709_TO_AP1, color);
	#endif

	// Color grading
	// ...

	// Apply tone mapping
	if (tonemapper_mode == 2)
		color = ACESFitted(color);
	else if (tonemapper_mode == 3)
		color = Uchimura(color, uchimura.max_brightness, uchimura.contrast, uchimura.linear_section_start, uchimura.linear_section_length, uchimura.black_tightness, uchimura.pedestal);

	#if USE_AP1
		color = mul(AP1_TO_REC709, color);
	#endif

	// Where Rec.709 input needed
	if (tonemapper_mode == 1)
		color = PBRNeutralToneMapping(color);
	else if (tonemapper_mode == 4)
		color = AgX(color);

	color = saturate(color);

	// Convert to sRGB
	float3 result = LinearToSRGB(color);

	output_texture[texel] = float4(result, 1.0);
}
