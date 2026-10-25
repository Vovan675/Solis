#pragma once
#include "bindless.h"

#define TONEMAP_LUT_SIZE 32

static const float TONEMAP_LUT_MIDDLE_GREY = 0.18;
static const float TONEMAP_LUT_STOPS = 20.0;

float3 linearToLog(float3 linear_color)
{
	return saturate(log2(linear_color / TONEMAP_LUT_MIDDLE_GREY) / TONEMAP_LUT_STOPS + 0.5);
}

float3 logToLinear(float3 log_color)
{
	return TONEMAP_LUT_MIDDLE_GREY * exp2((log_color - 0.5) * TONEMAP_LUT_STOPS);
}

float3 sampleTonemapLut(uint lut_tex_id, float3 linear_color)
{
	Texture3D lut = ResourceDescriptorHeap[lut_tex_id];
	float3 texel = linearToLog(linear_color) * (TONEMAP_LUT_SIZE - 1.0);
	float3 uvw = (texel + 0.5) / TONEMAP_LUT_SIZE;
	return lut.SampleLevel(linear_clamp_sampler, uvw, 0).rgb;
}

static const float UNCHARTED2_WHITE = 11.2;

// Uncharted 2 Tone Mapping
float3 Uncharted2(float3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
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
