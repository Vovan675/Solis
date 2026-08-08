#pragma once
#include "common.h"

// ============================================================================
// Microfacet BRDF Functions
// ============================================================================

// It's maybe faster and is okay even for x < 0 where hlsl's pow gives nan
float pow5(float x)
{
    float x2 = x * x;
    return x2 * x2 * x;
}

float3 FresnelSchlick(float3 f0, float3 f90, float u)
{
    return f0 + (f90 - f0) * pow5(1.0f - u);
}

float FresnelSchlick(float f0, float f90, float u)
{
    return f0 + (f90 - f0) * pow5(1.0f - u);
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

static const float MIN_PERCEPTUAL_ROUGHNESS = 0.0316; // sqrt(0.001) as mitsuba

#define DIELECTRIC_F0_FROSTBITE 0

float3 computeF0(float3 albedo, float metalness, float specular)
{
	#if DIELECTRIC_F0_FROSTBITE
		float dielectric_F0 = 0.16 * specular * specular;
	#else
		float dielectric_F0 = 0.08 * specular;
	#endif
	return lerp(dielectric_F0, albedo, metalness);
}

// Frostbite/Filament specular occlusion approximation
float computeSpecularAO(float NdotV, float ao, float alpha)
{
	return saturate(pow(NdotV + ao, exp2(-16.0 * alpha - 1.0)) - 1.0 + ao);
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