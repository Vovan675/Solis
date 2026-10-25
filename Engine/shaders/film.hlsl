#include "common.h"
#include "bindless.h"
#include "tonemap.h"

// Constant Buffer
cbuffer UBO : register(b0)
{
	uint composite_final_tex_id;
	float use_vignette;
	float vignette_radius;
	float vignette_smoothness;
	float exposure;
	uint tonemap_lut_tex_id;
	uint bloom_tex_id;
	float bloom_intensity;
};

// Vignette Calculation
float GetVignette(float2 uv, float radius, float smoothness) {
	float dist = radius - distance(uv, float2(0.5, 0.5));
	return smoothstep(-smoothness, smoothness, dist);
}

float4 PSMain(float2 uv : TEXCOORD0) : SV_TARGET {
	// Calculate vignette
	float vignette = 1.0;
	if (use_vignette > 0.5) {
		vignette = GetVignette(uv, vignette_radius, vignette_smoothness);
	}

	// Sample composite texture
	float4 compositeFinal = SampleTexture(composite_final_tex_id, uv);
	if (bloom_intensity > 0.0)
	{
		float3 bloom = SampleTextureLevel(bloom_tex_id, uv, 0, linear_clamp_sampler).rgb;
		compositeFinal.rgb += bloom / exposure * bloom_intensity;
	}

	float4 value = compositeFinal * vignette;

	value *= exposure;

	return float4(sampleTonemapLut(tonemap_lut_tex_id, value.rgb), 1.0);
}
