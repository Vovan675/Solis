
#pragma once
#include "common.h"

// From pbr book
float2 SampleUniformDiskConcentric(float2 u)
{
		float2 uOffset = 2 * u - float2(1, 1);
		if (uOffset.x == 0 && uOffset.y == 0)
				return float2(0, 0);

		float theta, r;
		if (abs(uOffset.x) > abs(uOffset.y)) {
				r = uOffset.x;
				theta = PI/4 * (uOffset.y / uOffset.x);
		} else {
				r = uOffset.y;
				theta = PI/2 - PI/4 * (uOffset.x / uOffset.y);
		}
		return r * float2(cos(theta), sin(theta));
}

float3 SampleCosineHemisphere(float2 u) {
		float2 d = SampleUniformDiskConcentric(u);
		float z = sqrt(max(0, 1 - d.x * d.x - d.y * d.y));
		return float3(d.x, d.y, z);
}

float3 UniformSampleHemisphere(float2 u) {
		float z = u[0];
		float r = sqrt(max(0, 1. - z * z));
		float phi = 2 * PI * u[1];
		return float3(r * cos(phi), r * sin(phi), z);
}

float GetBayer2x2(float2 pixelPosition)
{
	const float bayer_matrix_2x2[2][2] = {
		{  0.00,  1.00 },
		{  0.25,  0.75 },
	};
	return bayer_matrix_2x2[pixelPosition.x % 2][pixelPosition.y % 2];
}

float GetBayer4x4(float2 pixelPosition)
{
	const float bayer_matrix_4x4[4][4] = {
		{ 0.0,    0.5,    0.125,  0.625 },
		{ 0.75,   0.25,   0.875,  0.375 },
		{ 0.1875, 0.6875, 0.0625, 0.5625 },
		{ 0.9375, 0.4375, 0.8125, 0.3125 },
	};
	return bayer_matrix_4x4[pixelPosition.x % 4][pixelPosition.y % 4];
}

uint GetBayer4x4Uint(float2 pixelPosition)
{
	return GetBayer4x4(pixelPosition) * 16;
}

float GetBayer8x8(float2 pixelPosition)
{
	const float bayer_matrix_8x8[8][8] = {
		{ 0.000, 0.500, 0.125, 0.625, 0.03125, 0.53125, 0.15625, 0.65625 },
		{ 0.750, 0.250, 0.875, 0.375, 0.78125, 0.28125, 0.90625, 0.40625 },
		{ 0.1875, 0.6875, 0.0625, 0.5625, 0.21875, 0.71875, 0.09375, 0.59375 },
		{ 0.9375, 0.4375, 0.8125, 0.3125, 0.96875, 0.46875, 0.84375, 0.34375 },
		{ 0.015625, 0.515625, 0.140625, 0.640625, 0.046875, 0.546875, 0.171875, 0.671875 },
		{ 0.765625, 0.265625, 0.890625, 0.390625, 0.796875, 0.296875, 0.921875, 0.421875 },
		{ 0.203125, 0.703125, 0.078125, 0.578125, 0.234375, 0.734375, 0.109375, 0.609375 },
		{ 0.953125, 0.453125, 0.828125, 0.328125, 0.984375, 0.484375, 0.859375, 0.359375 },
	};
	return bayer_matrix_8x8[pixelPosition.x % 8][pixelPosition.y % 8];
}

// R2 is from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
// R2 Low discrepancy sequence
float2 R2(int index)
{
	static const float g  = 1.32471795724474602596f;
	static const float a1 = 1 / g;
	static const float a2 = 1 / (g * g);
	return float2(frac(float(index) * a1), frac(float(index) * a2));
}