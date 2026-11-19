#pragma once

RayDesc setupVisibilityRay(RAB_Surface surface, RAB_LightSample light_sample, float offset = 0.001)
{
	float3 origin = surface.world_position + surface.geometry_normal * offset * max(1.0, surface.view_depth);
	float3 to_light = light_sample.position - origin;

	RayDesc ray;
	ray.TMin = 0.0;
	ray.TMax = length(to_light) - offset;
	ray.Direction = normalize(to_light);
	ray.Origin = origin;
	return ray;
}

bool RAB_GetConservativeVisibility(RAB_Surface surface, RAB_LightSample light_sample)
{
	return traceVisibility(setupVisibilityRay(surface, light_sample));
}

bool RAB_GetTemporalConservativeVisibility(RAB_Surface current_surface, RAB_Surface previous_surface, RAB_LightSample light_sample)
{
	return RAB_GetConservativeVisibility(current_surface, light_sample);
}
