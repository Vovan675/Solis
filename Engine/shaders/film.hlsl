#include "common.h"
#include "bindless.h"

// Constant Buffer
cbuffer UBO : register(b0)
{
    uint composite_final_tex_id;
    float use_vignette;
    float vignette_radius;
    float vignette_smoothness;
    float exposure;
    int tonemapper_mode;
};

// Vignette Calculation
float GetVignette(float2 uv, float radius, float smoothness) {
    float dist = radius - distance(uv, float2(0.5, 0.5));
    return smoothstep(-smoothness, smoothness, dist);
}

// Uncharted 2 Tone Mapping
float3 Uncharted2(float3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// ACES Curve Fit Approximation
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
	{ 0.59719, 0.35458, 0.04823 },
	{ 0.07600, 0.90834, 0.01566 },
	{ 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
	{ 1.60475, -0.53108, -0.07367 },
	{ -0.10208,  1.10813, -0.00605 },
	{ -0.00327, -0.07276,  1.07602 }
};

float3 RRTAndODTFit(float3 v)
{
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}

float3 ACESFitted(float3 color)
{
	color = mul(ACESInputMat, color);

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color = mul(ACESOutputMat, color);

	// Clamp to [0, 1]
	color = saturate(color);

	return color;
}

float4 PSMain(float2 uv : TEXCOORD0) : SV_TARGET {
    // Calculate vignette
    float vignette = 1.0;
    if (use_vignette > 0.5) {
        vignette = GetVignette(uv, vignette_radius, vignette_smoothness);
    }

    // Sample composite texture
    float4 compositeFinal = SampleTexture(composite_final_tex_id, uv);
    float4 value = compositeFinal * vignette;

    value *= exposure;

    // Apply tone mapping
    if (tonemapper_mode == 1)
        value = float4(Uncharted2(value.rgb), 1.0);
    else if (tonemapper_mode == 2)
        value = float4(ACESFitted(value.rgb), 1.0);

    // Convert to sRGB
    value = LinearToSRGB(value);
    value = clamp(value, 0.0f, 1.0f);

    return float4(value.rgb, 1.0);
}