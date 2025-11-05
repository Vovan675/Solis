#include "../common.h"
#include "../bindless.h"
#include "../shading.h"
#include "ddgi_common.hlsl"


RWStructuredBuffer<float4> ray_data : register(u1);

RaytracingAccelerationStructure tlas : register(t2);

cbuffer Constants : register(b3)
{
	uint environment_tex_id;
};

struct RayPayload {
	bool hit;
	float depth;
	float2 bary;
	uint instance_id;
	uint primitive_id;
	uint hit_kind;
};

bool TraceShadowRay(float3 origin, float3 direction, float max_distance)
{
	RayDesc shadow_ray;
	shadow_ray.Origin = origin;
	shadow_ray.Direction = direction;
	shadow_ray.TMin = 0.0001;
	shadow_ray.TMax = max_distance;
	
	RayPayload shadow_payload;
	shadow_payload.hit = false;
	
	TraceRay(tlas, RAY_FLAG_NONE, 0xff, 0, 0, 0, shadow_ray, shadow_payload);
	
	return shadow_payload.hit;
}

[shader("raygeneration")]
void RayGen()
{
	uint ray_index = DispatchRaysIndex().x;
	uint probe_index = DispatchRaysIndex().y;

	StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
	DDGIVolume volume = volumes[0];
	
	uint3 probe_coords = GetProbeCoords(volume, probe_index);
	float3 probe_position = GetProbeWorldPosition(volume, probe_coords);
	float3 ray_direction = GetProbeRayDirection(ray_index, volume);

	#if USE_FIXED_RAYS
	if (volume.use_classification && ray_index >= NUM_FIXED_RAYS)
	#else
	if (volume.use_classification)
	#endif
	{
		if (IsProbeDisabled(volume, probe_coords))
			return;
	}

	RayDesc ray;
	ray.Origin = probe_position;
	ray.Direction = ray_direction;
	ray.TMin = 0.0;
	ray.TMax = 1000.0;

	RayPayload payload;
	payload.hit = false;
	
	TraceRay(tlas, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
	
	uint ray_data_index = GetRayDataIndex(probe_index, ray_index, volume);

	if (!payload.hit)
	{
		TextureCube environment_tex = ResourceDescriptorHeap[environment_tex_id];
		float3 environment = environment_tex.SampleLevel(linear_clamp_sampler, ray_direction, 0).rgb;
		ray_data[ray_data_index] = float4(saturate(environment), 10000);
		return;
	}

	if (payload.hit_kind == HIT_KIND_TRIANGLE_BACK_FACE)
	{
		// Mark as back face and shorten the ray to decrease influence during irradiance sampling
		ray_data[ray_data_index] = float4(0, 0, 0, -payload.depth * BACKFACE_DISTANCE_SCALE);
		return;
	}

	float3 radiance = 0;
	if (payload.hit)
	{
		Instance instance = GetInstance(payload.instance_id);
		Material material = GetMaterial(instance.material_id);

		Mesh mesh = GetMesh(instance.mesh_id);
		VertexData vertex = GetVertexData(mesh, payload.primitive_id, payload.bary);
		float3 albedo = (material.albedo_tex_id > 0) ? SampleTextureLevel(material.albedo_tex_id, vertex.uv, 0).rgb : material.albedo.rgb;
		radiance = albedo;

		float3 position = mul(instance.world_transform, float4(vertex.position, 1.0)).xyz;
		float3x3 normalMatrix = (float3x3)instance.world_transform;
		float3 normal = normalize(mul(normalMatrix, vertex.normal));
		bool visibility = !TraceShadowRay(position + normal * 0.01, volume.sun_dir.xyz, 10000.0);


		float3 diffuse = saturate(dot(normal, volume.sun_dir.xyz)) * LambertDiffuse(albedo);
		radiance = diffuse * visibility * volume.sun_color.rgb;
		//radiance = albedo;

		float volume_weight = GetVolumeWeight(position, volume);
		if (volume_weight > 0.0f)
		{
			float3 surface_bias = GetSurfaceBias(normal, ray.Direction);
			float3 irradiance = SampleIrradiance(position, normal, surface_bias, volume);
			radiance += (min(albedo, 0.9f) / PI) * irradiance * volume_weight;
		}
		//radiance = position;
	}

	ray_data[ray_data_index] = float4(radiance, payload.depth);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
	payload.hit = false;
	payload.depth = 1000;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
	payload.hit = true;
	payload.depth = RayTCurrent();
	payload.bary = attribs.barycentrics;
	payload.instance_id = InstanceID();
	payload.primitive_id = PrimitiveIndex();
	payload.hit_kind = HitKind();
}
