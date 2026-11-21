// Reference NVIDIA rtxpt
#include "../ray_tracing.h"
#include "../bindless.h"
#include "../sampling.h"
#include "../shading.h"
#include "../random.h"
#include "../path_tracing_utils.h"
#include "../lighting/lighting.h"

cbuffer Constants : register(b3)
{
	uint sun_light_index;
	uint accumulation_frame;
	uint environment_tex_id;
	uint output_tex_id;
	uint accumulation_tex_id;
};

struct SurfaceProperties
{
	float3 albedo;
	float3 emissive;
	float metalness;
	float roughness;
	float specular;
};

// ============================================================================
// Geometry
// ============================================================================

struct SurfaceHit
{
	float3 position;
	float3 normal;
	float3 specular_normal;
	float3 geometry_normal;
	float2 uv;
	Material material;
	uint instance_id;
	uint primitive_id;
	bool hit;
};

SurfaceHit TraceSurface(RaytracingAccelerationStructure tlas, RayDesc ray)
{
	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, RAY_TRACING_MASK_OPAQUE, ray);
	while (ray_query.Proceed())
	{
		if (!isAlphaClipped(ray_query.CandidateInstanceID(), ray_query.CandidatePrimitiveIndex(), ray_query.CandidateTriangleBarycentrics()))
			ray_query.CommitNonOpaqueTriangleHit();
	}
	
	SurfaceHit hit;
	hit.hit = ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
	
	if (hit.hit)
	{
		Instance instance = getInstance(ray_query.CommittedInstanceID());
		Mesh mesh = getMesh(instance.mesh_id);
		VertexData vertex = GetVertexData(mesh, ray_query.CommittedPrimitiveIndex(), ray_query.CommittedTriangleBarycentrics());
		
		float facing_sign = ray_query.CommittedTriangleFrontFace() ? 1.0 : -1.0;
		float3 geometry_normal = facing_sign * transformNormalToWorld(vertex.geometry_normal, instance.iworld_transform);
		float3 shading_normal = transformNormalToWorld(vertex.normal, instance.iworld_transform);

		hit.material = getMaterial(instance.material_id);
		if (hit.material.normal_tex_id > 0 && (mesh.attribute_flags & MESH_ATTR_TANGENT))
		{
			float4 tangent = float4(mul((float3x3)instance.world_transform, vertex.tangent.xyz), vertex.tangent.w);
			float3x3 tbn = computeTBN(shading_normal, tangent);
			shading_normal = applyNormalMap(SampleTextureLevel(hit.material.normal_tex_id, vertex.uv, 0).rg, tbn);
		}
		shading_normal *= facing_sign;

		hit.position = mul(instance.world_transform, float4(vertex.position, 1.0)).xyz;
		hit.normal = shading_normal;
		hit.specular_normal = adjustShadingNormal(shading_normal, geometry_normal, ray.Direction);
		hit.geometry_normal = geometry_normal;
		hit.uv = vertex.uv;
		hit.instance_id = ray_query.CommittedInstanceID();
		hit.primitive_id = ray_query.CommittedPrimitiveIndex();
	}
	
	return hit;
}

bool TraceShadowRay(float3 origin, float3 direction, float max_distance)
{
	RayDesc shadow_ray;
	shadow_ray.Origin = origin;
	shadow_ray.Direction = direction;
	shadow_ray.TMin = 0.0001;
	shadow_ray.TMax = max_distance;
	
	return !traceVisibility(shadow_ray);
}

// ============================================================================
// Material
// ============================================================================

SurfaceProperties EvaluateMaterial(Material material, float2 uv)
{
	SurfaceProperties props;
	props.albedo = material.albedo.rgb;
	if (material.albedo_tex_id > 0)
		props.albedo *= SampleTextureLevel(material.albedo_tex_id, uv, 0).rgb;
	props.emissive = material.emissive.rgb;
	if (material.emissive_tex_id > 0)
		props.emissive *= SampleTextureLevel(material.emissive_tex_id, uv, 0).rgb;
	props.metalness = material.shading.r;
	if (material.metalness_tex_id > 0)
		props.metalness *= SampleTextureLevel(material.metalness_tex_id, uv, 0).b;
	props.roughness = material.shading.g;
	if (material.roughness_tex_id > 0)
		props.roughness *= SampleTextureLevel(material.roughness_tex_id, uv, 0).g;
	props.roughness = max(props.roughness, MIN_PERCEPTUAL_ROUGHNESS);
	props.specular = material.shading.b;
	if (material.specular_tex_id > 0)
		props.specular *= SampleTextureLevel(material.specular_tex_id, uv, 0).r;
	return props;
}

static const float DECAL_RAY_BIAS = 0.05;

void ApplyDecals(RaytracingAccelerationStructure tlas, inout SurfaceProperties surface, float3 position, float3 direction)
{
	RayDesc ray;
	ray.Origin = position - direction * DECAL_RAY_BIAS;
	ray.Direction = direction;
	ray.TMin = 0.0;
	ray.TMax = 2.0 * DECAL_RAY_BIAS;

	RayQuery<RAY_FLAG_FORCE_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, RAY_TRACING_MASK_DECAL, ray);
	while (ray_query.Proceed())
	{
		Instance instance = getInstance(ray_query.CandidateInstanceID());
		Material material = getMaterial(instance.material_id);
		float2 uv = getTriangleUV(getMesh(instance.mesh_id), ray_query.CandidatePrimitiveIndex(), ray_query.CandidateTriangleBarycentrics());

		float4 albedo = material.albedo;
		if (material.albedo_tex_id > 0)
			albedo *= SampleTextureLevel(material.albedo_tex_id, uv, 0);

		if (material.flags & MATERIAL_FLAG_DECAL_WRITE_ALBEDO)
			surface.albedo = lerp(surface.albedo, albedo.rgb, albedo.a);
		if (material.flags & MATERIAL_FLAG_DECAL_WRITE_SHADING)
		{
			SurfaceProperties decal = EvaluateMaterial(material, uv);
			surface.metalness = lerp(surface.metalness, decal.metalness, albedo.a);
			surface.roughness = lerp(surface.roughness, decal.roughness, albedo.a);
			surface.specular = lerp(surface.specular, decal.specular, albedo.a);
		}
	}
}

// ============================================================================
// BRDF
// ============================================================================

float3 EvaluateBRDFCosine(SurfaceProperties surface, float3 L, float3 V, float3 N, float3 specular_normal)
{
	float3 H = normalize(L + V);
	
	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);
	float specular_NdotL = max(dot(specular_normal, L), 0.0);
	float specular_NdotV = max(dot(specular_normal, V), 0.0);
	float specular_NdotH = max(dot(specular_normal, H), 0.0);
	float VdotH = max(dot(V, H), 0.0);
	
	float3 F0 = computeF0(surface.albedo, surface.metalness, surface.specular);
	float3 F = FresnelSchlick(F0, 1.0f, VdotH);
	
	// Specular (Cook-Torrance)
	float alpha = surface.roughness * surface.roughness;
	float D = D_GGX(specular_NdotH, alpha * alpha);
	float G = V_SmithGGXCorrelated(specular_NdotV, specular_NdotL, alpha);
	float3 specular = specular_NdotL > 0.0 && specular_NdotV > 0.0 ? F * D * G * specular_NdotL : 0.0;

	// Diffuse (Disney)
	float3 diffuse_color = surface.albedo * (1.0 - surface.metalness);
	float3 diffuse = diffuse_color * Fr_DisneyDiffuse(NdotV, NdotL, VdotH, surface.roughness) / PI;
	
	return diffuse * NdotL + specular;
}

struct MaterialSample
{
	float3 direction;
	float3 throughput;
	float pdf;
	float lobe_probability;
};

MaterialSample SampleBRDF(SurfaceProperties surface, float3 V, float3 N, float3 specular_normal, inout RandomState rng)
{
	MaterialSample result;
	
	float3 T, B;
	BuildOrthonormalBasis(N, T, B);
	
	float3 F0 = computeF0(surface.albedo, surface.metalness, surface.specular);
	float NdotV = max(dot(N, V), 0.0);
	float3 F = FresnelSchlick(F0, 1.0f, NdotV);
	
	float specular_prob = Luminance(F);
	specular_prob = max(specular_prob, surface.roughness * surface.roughness);
	specular_prob = clamp(specular_prob, 0.1, 0.9);
	
	bool sample_specular = Random(rng) < specular_prob;
	
	if (sample_specular)
	{
		// Sample GGX specular lobe
		float3 H_tangent = SampleGGXTangent(Random2(rng), surface.roughness);
		float3 specular_T;
		float3 specular_B;
		BuildOrthonormalBasis(specular_normal, specular_T, specular_B);
		float3 H = TangentToWorld(H_tangent, specular_T, specular_B, specular_normal);
		float3 L = reflect(-V, H);
		
		if (dot(specular_normal, L) <= 0.0)
		{
			// Fallback to diffuse
			float3 L_tangent = SampleCosineHemisphere(Random2(rng));
			L = TangentToWorld(L_tangent, T, B, N);
		}
		
		result.direction = L;
		result.lobe_probability = specular_prob;
		
		float3 brdf_cosine = EvaluateBRDFCosine(surface, L, V, N, specular_normal);
		float NdotH = max(dot(specular_normal, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float spec_pdf = GGX_PDF(NdotH, VdotH, surface.roughness);
		float diff_pdf = max(dot(N, L), 0.0) / PI;
		float pdf = specular_prob * spec_pdf + (1.0 - specular_prob) * diff_pdf;
		
		result.pdf = max(pdf, Epsilon);
		result.throughput = brdf_cosine / result.pdf;
	}
	else
	{
		// Sample cosine-weighted diffuse lobe
		float3 L_tangent = SampleCosineHemisphere(Random2(rng));
		float3 L = TangentToWorld(L_tangent, T, B, N);
		
		result.direction = L;
		result.lobe_probability = 1.0 - specular_prob;
		
		float NdotL = max(dot(N, L), 0.0);
		float3 brdf_cosine = EvaluateBRDFCosine(surface, L, V, N, specular_normal);
		
		float3 H = normalize(L + V);
		float NdotH = max(dot(specular_normal, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float spec_pdf = GGX_PDF(NdotH, VdotH, surface.roughness);
		float diff_pdf = NdotL / PI;
		float pdf = specular_prob * spec_pdf + (1.0 - specular_prob) * diff_pdf;
		
		result.pdf = max(pdf, Epsilon);
		result.throughput = brdf_cosine / result.pdf;
	}
	
	return result;
}

// ============================================================================
// Lighting
// ============================================================================

float3 SampleDirectLighting(SurfaceHit hit, SurfaceProperties surface, float3 V, inout RandomState rng)
{
	float3 N = hit.normal;
	float3 shadow_origin = hit.position + hit.geometry_normal * 0.001;

	float3 radiance = 0;
	for (uint light_index = 0; light_index < lights_count; light_index++)
	{
		Light light = getLight(light_index);
		float3 L;
		float attenuation = getLightAttenuation(light, hit.position, L);
		if (light.type == LIGHT_TYPE_DIRECTIONAL)
			L = sampleSunDirection(light, Random2(rng));

		float3 brdf_cosine = EvaluateBRDFCosine(surface, L, V, N, hit.specular_normal);
		if (attenuation <= 0.0 || all(brdf_cosine <= 0.0))
			continue;

		float max_distance = light.type == LIGHT_TYPE_DIRECTIONAL ? 10000.0 : distance(light.position.xyz, hit.position);
		if (TraceShadowRay(shadow_origin, L, max_distance))
			continue;

		radiance += brdf_cosine * attenuation * light.radiance.rgb;
	}
	
	return radiance;
}

float3 SampleEnvironment(float3 direction)
{
	if (environment_tex_id == 0)
		return 0;

	TextureCube environment_tex = ResourceDescriptorHeap[environment_tex_id];
	return environment_tex.SampleLevel(linear_clamp_sampler, direction, 0).rgb * sky_intensity;
}

// ============================================================================
// Camera
// ============================================================================

RayDesc GenerateCameraRay(uint2 pixel, uint2 screen_size)
{
	float2 pixel_center = pixel + 0.5f;
	float2 uv = pixel_center / screen_size;
	float2 ndc = uv * 2.0f - 1.0f;
	ndc.y *= -1.0f;
	
	float4x4 view_projection_inverse = mul(iview, iprojection);

	float4 ray_start = mul(view_projection_inverse, float4(ndc, 1, 1));
	ray_start.xyz /= ray_start.w;

	float4 ray_end = mul(view_projection_inverse, float4(ndc, 0, 1));
	ray_end.xyz /= ray_end.w;
	
	RayDesc ray;
	ray.Origin = camera_position.xyz;
	ray.Direction = normalize(ray_end.xyz - ray_start.xyz);
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	
	return ray;
}

void AccumulateAndOutput(uint2 pixel, float3 radiance)
{
	RWTexture2D<float4> output = ResourceDescriptorHeap[output_tex_id];
	RWTexture2D<float4> accumulation = ResourceDescriptorHeap[accumulation_tex_id];

	float4 accumulated = accumulation_frame > 0 ? accumulation[pixel] : float4(0, 0, 0, 0);
	accumulated += float4(radiance, 0.0f);
	accumulation[pixel] = accumulated;
	
	float3 average = accumulated.rgb / float(accumulation_frame + 1);
	output[pixel] = float4(average, 1.0f);
}

// ============================================================================
// Path Tracing
// ============================================================================

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	uint2 screen_size = uint2(render_resolution.xy);
	if (any(pixel >= screen_size))
		return;

	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];
	
	RandomState rng = InitRandomState(pixel.x + pixel.y * screen_size.x, frame);
	RayDesc ray = GenerateCameraRay(pixel, screen_size);
	
	const int MAX_BOUNCES = 12;
	const float WHITE_LUMINANCE = camera_exposure > 0.0 ? 1.0 / camera_exposure : 1.0;
	const float BASE_FIREFLY_THRESHOLD = 3.0 * WHITE_LUMINANCE;
	
	float3 radiance = float3(0, 0, 0);
	float3 throughput = float3(1, 1, 1);
	float rr_correction = 1.0;
	float fireflyFilterK = 1.0;
	
	for (int bounce = 0; bounce < MAX_BOUNCES; bounce++)
	{
		throughput *= rr_correction;
		rr_correction = 1.0;
		
		SurfaceHit hit = TraceSurface(tlas, ray);
		
		if (!hit.hit)
		{
			float3 env = SampleEnvironment(ray.Direction);
			if (bounce == 0 && sun_light_index != INVALID_LIGHT_INDEX)
			{
				Light sun = getLight(sun_light_index);
				env += getSunDisk(ray.Direction, normalize(sun.direction.xyz), sun.radiance.rgb, sun.angular_radius);
			}

			env *= throughput;
			radiance += bounce > 0 ? FireflyFilter(env, BASE_FIREFLY_THRESHOLD, fireflyFilterK) : env;
			break;
		}
		
		SurfaceProperties surface = EvaluateMaterial(hit.material, hit.uv);
		ApplyDecals(tlas, surface, hit.position, ray.Direction);
		
		// Direct lighting with relaxed firefly threshold
		float3 direct = throughput * SampleDirectLighting(hit, surface, -ray.Direction, rng);
		direct += throughput * surface.emissive;
		radiance += bounce > 0 ? FireflyFilter(direct, BASE_FIREFLY_THRESHOLD * 6.0, fireflyFilterK) : direct;
		
		// Sample BRDF and update path
		MaterialSample brdf_sample = SampleBRDF(surface, -ray.Direction, hit.normal, hit.specular_normal, rng);
		throughput *= brdf_sample.throughput;
		
		// Update firefly filter based on PDF and lobe probability
		fireflyFilterK = ComputeNewScatterFireflyFilterK(
			fireflyFilterK, 
			brdf_sample.pdf, 
			brdf_sample.lobe_probability
		);
		
		// Russian Roulette termination
		if (HandleRussianRoulette(throughput, bounce, MAX_BOUNCES, rr_correction, rng))
			break;
		
		if (max(throughput.r, max(throughput.g, throughput.b)) < 0.001)
			break;
		
		ray.Origin = hit.position + hit.geometry_normal * 0.001;
		ray.Direction = brdf_sample.direction;
		ray.TMin = 0.001;
		ray.TMax = 10000.0;
	}
	

	radiance = min(radiance, 100.0 * WHITE_LUMINANCE);

	AccumulateAndOutput(pixel, radiance);
}
