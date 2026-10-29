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

struct ColorGradingParameters
{
	float temperature;
	float tint;
	float contrast;
	float saturation;
	float shadows;
	float midtones;
	float highlights;
};

cbuffer Constants : register(b0)
{
	uint output_tex_id;
	int tonemapper_mode;
	UchimuraParameters uchimura;
	ColorGradingParameters color_grading;
};

// Y in XYZ is luminance
static const float3 WORKING_LUMINANCE = USE_AP1 ? AP1_TO_XYZ[1] : REC709_TO_XYZ[1];

[[vk::image_format("rgba16f")]]
static RWTexture3D<float4> output_texture = ResourceDescriptorHeap[output_tex_id];

[numthreads(4, 4, 4)]
void CSMain(uint3 texel : SV_DispatchThreadID)
{
	float3 color = logToLinear(texel / (TONEMAP_LUT_SIZE - 1.0));

	color = whiteBalance(color, color_grading.temperature, color_grading.tint);

	#if USE_AP1
		color = mul(REC709_TO_AP1, color);
	#endif

	// Contrast, shadows, midtones and highlights in stops of luminance around grey
	float luminance = dot(color, WORKING_LUMINANCE);
	float ev = log2(max(luminance, 1e-10) / MIDDLE_GREY); // How much EV is luminance far from middle gray

	float shadows_weight = 1.0 - smoothstep(-4.0, -1.0, ev);
	float highlights_weight = smoothstep(1.0, 4.0, ev);
	float midtones_weight = 1.0 - shadows_weight - highlights_weight;

	float graded_ev = ev * color_grading.contrast; // Move EV nearer of farther from gray based on constrast
	graded_ev += color_grading.shadows * shadows_weight;
	graded_ev += color_grading.highlights * highlights_weight;
	graded_ev += color_grading.midtones * midtones_weight;

	color *= exp2(graded_ev - ev);

	// Saturation (lerp from luminance to color, or even beyond color)
	luminance = dot(color, WORKING_LUMINANCE);
	color = max(lerp(luminance, color, color_grading.saturation), 0.0);

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
