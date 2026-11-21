#pragma once

#include "RAB_Material.hlsli"
#include "RAB_Surface.hlsli"

float2 RAB_GetEnvironmentMapRandXYFromDir(float3 world_direction)
{
	return float2(0.0, 0.0);
}

float RAB_EvaluateEnvironmentMapSamplingPdf(float3 L)
{
	return 0.0;
}

float RAB_EvaluateLocalLightSourcePdf(uint light_index)
{
	return 1.0 / constants.light_buffer_params.localLightBufferRegion.numLights;
}

void evaluateBrdf(RAB_Surface surface, float3 sample_position, out float3 diffuse, out float3 specular)
{
	diffuse = 0;
	specular = 0;

	float3 L = normalize(sample_position - surface.world_position);

	if (dot(L, surface.geometry_normal) <= 0)
		return;

	RAB_Material material = RAB_GetMaterial(surface);
	shadeDirectLight(L, 1.0, surface.normal, surface.view_direction, material.albedo, material.metalness, material.roughness, material.specular, diffuse, specular);
}

float RAB_GetLightSampleTargetPdfForSurface(RAB_LightSample light_sample, RAB_Surface surface)
{
	if (light_sample.solid_angle_pdf <= 0)
		return 0;

	float3 diffuse;
	float3 specular;
	evaluateBrdf(surface, light_sample.position, diffuse, specular);
	return Luminance((diffuse + specular) * light_sample.radiance) / light_sample.solid_angle_pdf;
}

void RAB_GetLightDirDistance(RAB_Surface surface, RAB_LightSample light_sample, out float3 light_direction, out float light_distance)
{
	float3 to_light = light_sample.position - surface.world_position;
	light_distance = length(to_light);
	light_direction = to_light / light_distance;
}

bool RAB_TraceRayForLocalLight(float3 origin, float3 direction, float t_min, float t_max, out uint light_index, out float2 random_xy)
{
	light_index = RTXDI_InvalidLightIndex;
	random_xy = 0;

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = t_min;
	ray.TMax = t_max;

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];
	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, RAY_TRACING_MASK_SHADOW, ray);
	while (ray_query.Proceed())
	{
		if (!isAlphaClipped(ray_query.CandidateInstanceID(), ray_query.CandidatePrimitiveIndex(), ray_query.CandidateTriangleBarycentrics()))
			ray_query.CommitNonOpaqueTriangleHit();
	}

	bool hit = ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
	// TODO: handle emissive triangle hit and set light_index, random_xy

	return hit;
}

RAB_LightSample RAB_SamplePolymorphicLight(RAB_LightInfo light_info, RAB_Surface surface, float2 uv)
{
	RAB_LightSample result;
	float3 L;
	float attenuation = getLightAttenuation(light_info, surface.world_position, L);
	if (light_info.type == LIGHT_TYPE_DIRECTIONAL)
	{
		L = sampleSunDirection(light_info, uv);
		result.position = surface.world_position + L * 10000.0;
	} else
	{
		result.position = light_info.position.xyz;
	}
	result.normal = -L;
	result.radiance = light_info.radiance.rgb * attenuation;
	result.solid_angle_pdf = 1.0;
	return result;
}
