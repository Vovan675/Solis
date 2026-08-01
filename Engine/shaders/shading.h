#pragma once
#include "common.h"

// ============================================================================
// Microfacet BRDF Functions
// ============================================================================

float3 FresnelSchlick(float3 f0, float3 f90, float u)
{
    return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

float FresnelSchlick(float f0, float f90, float u)
{
    return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

// On edges shading normal may be back faced to ray direction. Simple fix is to just flip normal in this case.
// But there is better way, without discontinuity, https://iquilezles.org/articles/dontflip
float3 adjustShadingNormal(float3 shading_normal, float3 geometry_normal, float3 ray_direction)
{
	float3 reflected = reflect(ray_direction, shading_normal);
	float k = dot(reflected, geometry_normal);
	if (k >= 0.0)
		return shading_normal;
	return normalize(normalize(reflected - k * geometry_normal) - ray_direction);
}

float3 ComputeF0(float3 albedo, float metalness)
{
	float3 dielectric_F0 = float3(0.04, 0.04, 0.04);
	return lerp(dielectric_F0, albedo, metalness);
}

float D_GGX(float NdotH, float a2)
{
    float f = (NdotH * a2 - NdotH) * NdotH + 1.0;
    return a2 / (PI * f * f);
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness)
{
    float alphaRoughnessSq = roughness * roughness;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGX = GGXV + GGXL;
    return GGX > 0.0 ? 0.5 / GGX : 0.0;
}

float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
    float energyBias = lerp(0, 0.5, linearRoughness);
    float energyFactor = lerp(1.0, 1.0 / 1.51, linearRoughness);
    float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
    float f0 = 1.0;
    float lightScatter = FresnelSchlick(f0, fd90, NdotL).r;
    float viewScatter = FresnelSchlick(f0, fd90, NdotV).r;
    return lightScatter * viewScatter * energyFactor;
}

float3 LambertDiffuse(float3 albedo)
{
	return albedo / PI;
}

// ============================================================================
// GGX Importance Sampling
// ============================================================================

float3 SampleGGXTangent(float2 random_sample, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	
	float phi = 2.0 * PI * random_sample.x;
	float cosTheta = sqrt((1.0 - random_sample.y) / (1.0 + (a2 - 1.0) * random_sample.y));
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
	
	return float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

float GGX_PDF(float NdotH, float VdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float D = D_GGX(NdotH, a2);
	return D * NdotH / (4.0 * VdotH);
}