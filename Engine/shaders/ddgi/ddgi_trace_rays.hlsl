#include "../ray_tracing.h"
#include "../bindless.h"
#include "../lighting/lighting.h"
#include "ddgi_common.hlsl"

// Always trace infinite rays, as last cascade
#define USE_INFINITE_RAYS 1

cbuffer Constants : register(b1)
{
	uint output_buffer_id;
	uint environment_tex_id;
	uint probes_to_update_buffer_id;
};

bool TraceShadowRay(float3 origin, float3 direction, float max_distance)
{
	RayDesc shadow_ray;
	shadow_ray.Origin = origin;
	shadow_ray.Direction = direction;
	shadow_ray.TMin = 0.0001;
	shadow_ray.TMax = max_distance;

	return !traceVisibility(shadow_ray, RAY_FLAG_FORCE_OPAQUE);
}

// 400 rays traced, and info saved as ray_data
// Then it will be used to generate distance/irradiance atlases

[numthreads(64, 1, 1)]
void CS_TraceRays(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	uint ray_index = dispatch_thread_id.x;

	DDGIVolume volume = ddgi_volume;
	if (ray_index >= volume.rays_per_probe)
		return;

	StructuredBuffer<uint> probes_to_update = ResourceDescriptorHeap[probes_to_update_buffer_id];
	uint probe_id = probes_to_update[dispatch_thread_id.y];

	uint cascade_id = GetProbeCascade(volume, probe_id);
	DDGICascade cascade = volume.cascades[cascade_id];
	uint3 probe_coords = GetProbeGridCoords(volume, probe_id);
	float3 probe_position = GetProbeTracePosition(volume, probe_coords, cascade_id);
	float3 ray_direction = GetProbeRayDirection(ray_index, volume);

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];

	float ray_distance = 10000.0f;

	#if USE_INFINITE_RAYS == 0
		// Need radiance cache of the next cascade on miss for real use
		bool is_not_last_cascade = cascade_id < (volume.cascades_count - 1);
		if (is_not_last_cascade)
		{
			float next_cascade_spacing = max(cascade.spacing.x, max(cascade.spacing.y, cascade.spacing.z)) * 2.0f;
			ray_distance = next_cascade_spacing * 2.0f;
		}
	#endif

	RayDesc ray;
	ray.Origin = probe_position;
	ray.Direction = ray_direction;
	ray.TMin = 0.0;
	ray.TMax = ray_distance;

	RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, 0xff, ray);
	ray_query.Proceed();

	uint ray_data_index = GetRayDataIndex(dispatch_thread_id.y, ray_index, volume);

	RWStructuredBuffer<float4> ray_data = ResourceDescriptorHeap[output_buffer_id];

	if (ray_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
	{
		#if USE_INFINITE_RAYS == 0
			if (is_not_last_cascade)
			{
				float3 position = ray.Origin + ray.Direction * ray.TMax;
				float3 normal = ray.Direction;
				float3 irradiance = SampleIrradiance(position, normal, float3(0, 0, 0), volume, cascade_id + 1);
				ray_data[ray_data_index] = float4(irradiance / PI, ray.TMax);
			} else
		#endif
		{
			float3 environment = 0;
			if (environment_tex_id != 0)
			{
				TextureCube environment_tex = ResourceDescriptorHeap[environment_tex_id];
				environment = environment_tex.SampleLevel(linear_clamp_sampler, ray_direction, 0).rgb * sky_intensity;
			}
			ray_data[ray_data_index] = float4(environment, MISS_DISTANCE);
		}
		return;
	}

	float hit_distance = ray_query.CommittedRayT();
	if (!ray_query.CommittedTriangleFrontFace())
	{
		// Mark as back face and shorten the ray to decrease influence during irradiance sampling
		ray_data[ray_data_index] = float4(0, 0, 0, -hit_distance * BACKFACE_DISTANCE_SCALE);
		return;
	}

	Instance instance = getInstance(ray_query.CommittedInstanceID());
	Material material = getMaterial(instance.material_id);

	Mesh mesh = getMesh(instance.mesh_id);
	VertexData vertex = GetVertexData(mesh, ray_query.CommittedPrimitiveIndex(), ray_query.CommittedTriangleBarycentrics());
	float3 albedo = (material.albedo_tex_id > 0) ? SampleTextureLevel(material.albedo_tex_id, vertex.uv, 0).rgb : material.albedo.rgb;
	float metalness = (material.metalness_tex_id > 0) ? SampleTextureLevel(material.metalness_tex_id, vertex.uv, 0).b : material.shading.r;
	float3 diffuse_color = albedo * (1.0f - metalness);

	float3 position = mul(instance.world_transform, float4(vertex.position, 1.0)).xyz;
	float3 normal = transformNormalToWorld(vertex.normal, instance.iworld_transform);
	
	float3 diffuse = LambertDiffuse(diffuse_color);
	float3 shadow_origin = position + normal * 0.01;

	float3 radiance = material.emissive.rgb;
	if (material.emissive_tex_id > 0)
		radiance *= SampleTextureLevel(material.emissive_tex_id, vertex.uv, 0).rgb;
	for (uint light_index = 0; light_index < lights_count; light_index++)
	{
		Light light = getLight(light_index);
		float3 L;
		float attenuation = getLightAttenuation(light, position, L);
		float NdotL = saturate(dot(normal, L));
		if (attenuation * NdotL <= 0.0)
			continue;

		float max_distance = light.type == LIGHT_TYPE_DIRECTIONAL ? 10000.0 : distance(light.position.xyz, position);
		if (TraceShadowRay(shadow_origin, L, max_distance))
			continue;

		radiance += diffuse * NdotL * attenuation * light.radiance.rgb;
	}

	float volume_weight = GetVolumeWeight(position, volume);
	if (volume_weight > 0.0f)
	{
		// Dithered cascade transition (for not sampling probes twice)
		uint bounce_cascade_id;
		float cascade_weight = GetCascadeWeight(position, volume, bounce_cascade_id);
		if (hash13(float3(ray_index, probe_id, 0)) > cascade_weight)
			bounce_cascade_id++;

		// Bias just towards probe center. Up to 0.1 of spacing, not more than half of depth
		float3 cascade_spacing = volume.cascades[bounce_cascade_id].spacing.xyz;
		float spacing = min(cascade_spacing.x, min(cascade_spacing.y, cascade_spacing.z));
		float3 surface_bias = -ray.Direction * min(0.1f * spacing, hit_distance * 0.5f);
		
		float3 irradiance = SampleIrradiance(position, normal, surface_bias, volume, bounce_cascade_id);
		radiance += (min(diffuse_color, 0.9f) / PI) * irradiance * volume_weight;
	}

	ray_data[ray_data_index] = float4(radiance, hit_distance);
}
