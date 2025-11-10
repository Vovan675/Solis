// Reference NVIDIA rtxpt
#include "../common.h"
#include "../bindless.h"
#include "../sampling.h"
#include "../shading.h"
#include "../random.h"
#include "../path_tracing_utils.h"

cbuffer Light : register(b3)
{
	float4 dir_light_direction;
	float4 dir_light_color;
	uint accumulation_frame;
	uint environment_tex_id;
	uint output_tex_id;
	uint accumulation_tex_id;
};

struct RayPayload {
	bool hit;
	float depth;
	float2 bary;
	uint instance_id;
	uint primitive_id;
};

struct SurfaceProperties
{
	float3 albedo;
	float metalness;
	float roughness;
};

// ============================================================================
// Geometry
// ============================================================================

struct SurfaceHit
{
	float3 position;
	float3 normal;
	float2 uv;
	uint instance_id;
	uint primitive_id;
	bool hit;
};

SurfaceHit TraceSurface(RaytracingAccelerationStructure tlas, RayDesc ray)
{
	RayPayload payload;
	payload.hit = false;
	
	TraceRay(tlas, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);
	
	SurfaceHit hit;
	hit.hit = payload.hit;
	
	if (payload.hit)
	{
		Instance instance = GetInstance(payload.instance_id);
		Mesh mesh = GetMesh(instance.mesh_id);
		VertexData vertex = GetVertexData(mesh, payload.primitive_id, payload.bary);
		
		hit.position = mul(instance.world_transform, float4(vertex.position, 1.0)).xyz;
		float3x3 normalMatrix = (float3x3)instance.world_transform;
		hit.normal = normalize(mul(normalMatrix, vertex.normal));
		hit.uv = vertex.uv;
		hit.instance_id = payload.instance_id;
		hit.primitive_id = payload.primitive_id;
	}
	
	return hit;
}

bool TraceShadowRay(RaytracingAccelerationStructure tlas, float3 origin, float3 direction, float max_distance)
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

// ============================================================================
// Material
// ============================================================================

SurfaceProperties EvaluateMaterial(Material material, float2 uv)
{
	SurfaceProperties props;
	props.albedo = (material.albedo_tex_id > 0) ? SampleTextureLevel(material.albedo_tex_id, uv, 0).rgb : material.albedo.rgb;
	props.metalness = (material.metalness_tex_id > 0) ? SampleTextureLevel(material.metalness_tex_id, uv, 0).r : material.shading.r;
	props.roughness = (material.roughness_tex_id > 0) ? SampleTextureLevel(material.roughness_tex_id, uv, 0).r : material.shading.g;
	props.roughness = max(props.roughness, 0.045); // Clamp for preventing division by zero
	return props;
}

// ============================================================================
// BRDF
// ============================================================================

float3 EvaluateBRDF(SurfaceProperties surface, float3 L, float3 V, float3 N)
{
	float3 H = normalize(L + V);
	
	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float VdotH = max(dot(V, H), 0.0);
	
	if (NdotL <= 0.0 || NdotV <= 0.0)
		return float3(0, 0, 0);
	
	float3 F0 = ComputeF0(surface.albedo, surface.metalness);
	float3 F = FresnelSchlick(F0, 1.0f, VdotH);
	
	// Specular (Cook-Torrance)
	float a = surface.roughness * surface.roughness;
	float a2 = a * a;
	float D = D_GGX(NdotH, a2);
	float G = V_SmithGGXCorrelated(NdotV, NdotL, surface.roughness);
	float3 specular = F * D * G;
	
	// Diffuse (energy-conserving Lambertian)
	float3 kD = (1.0 - F) * (1.0 - surface.metalness);
	float3 diffuse = kD * surface.albedo / PI;
	
	return diffuse + specular;
}

struct MaterialSample
{
	float3 direction;
	float3 throughput;
	float pdf;
	float lobe_probability;
};

MaterialSample SampleBRDF(SurfaceProperties surface, float3 V, float3 N, inout RandomState rng)
{
	MaterialSample result;
	
	float3 T, B;
	BuildOrthonormalBasis(N, T, B);
	
	float3 F0 = ComputeF0(surface.albedo, surface.metalness);
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
		float3 H = TangentToWorld(H_tangent, T, B, N);
		float3 L = reflect(-V, H);
		
		float NdotL = dot(N, L);
		if (NdotL <= 0.0)
		{
			// Fallback to diffuse
			float3 L_tangent = SampleCosineHemisphere(Random2(rng));
			L = TangentToWorld(L_tangent, T, B, N);
			NdotL = dot(N, L);
		}
		
		result.direction = L;
		result.lobe_probability = specular_prob;
		
		float3 brdf = EvaluateBRDF(surface, L, V, N);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float spec_pdf = GGX_PDF(NdotH, VdotH, surface.roughness);
		float diff_pdf = NdotL / PI;
		float pdf = specular_prob * spec_pdf + (1.0 - specular_prob) * diff_pdf;
		
		result.pdf = max(pdf, Epsilon);
		result.throughput = brdf * NdotL / result.pdf;
	}
	else
	{
		// Sample cosine-weighted diffuse lobe
		float3 L_tangent = SampleCosineHemisphere(Random2(rng));
		float3 L = TangentToWorld(L_tangent, T, B, N);
		
		result.direction = L;
		result.lobe_probability = 1.0 - specular_prob;
		
		float NdotL = max(dot(N, L), 0.0);
		float3 brdf = EvaluateBRDF(surface, L, V, N);
		
		float3 H = normalize(L + V);
		float NdotH = max(dot(N, H), 0.0);
		float VdotH = max(dot(V, H), 0.0);
		float spec_pdf = GGX_PDF(NdotH, VdotH, surface.roughness);
		float diff_pdf = NdotL / PI;
		float pdf = specular_prob * spec_pdf + (1.0 - specular_prob) * diff_pdf;
		
		result.pdf = max(pdf, Epsilon);
		result.throughput = brdf * NdotL / result.pdf;
	}
	
	return result;
}

// ============================================================================
// Lighting
// ============================================================================

float3 SampleDirectLighting(RaytracingAccelerationStructure tlas, SurfaceHit hit, SurfaceProperties surface, float3 V, inout RandomState rng)
{
	float3 L = dir_light_direction.xyz;
	float3 N = hit.normal;
	
	float NdotL = dot(N, L);
	if (NdotL <= 0.0)
		return float3(0, 0, 0);
	
	float3 shadow_origin = hit.position + N * 0.001;
	if (TraceShadowRay(tlas, shadow_origin, L, 10000.0))
		return float3(0, 0, 0);
	
	float3 brdf = EvaluateBRDF(surface, L, V, N);
	float3 light_color = dir_light_color.rgb;
	
	return brdf * light_color * NdotL;
}

float3 SampleEnvironment(float3 direction)
{
	TextureCube environment_tex = ResourceDescriptorHeap[environment_tex_id];
	return environment_tex.SampleLevel(linear_clamp_sampler, direction, 0).rgb;
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
	
	float4 ray_start = mul(view_projection_inverse, float4(ndc, 0, 1));
	ray_start.xyz /= ray_start.w;
	
	float4 ray_end = mul(view_projection_inverse, float4(ndc, 1, 1));
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

[shader("raygeneration")]
void RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];

	uint2 pixel = DispatchRaysIndex().xy;
	uint2 screen_size = DispatchRaysDimensions().xy;
	
	RandomState rng = InitRandomState(pixel.x + pixel.y * screen_size.x, frame);
	RayDesc ray = GenerateCameraRay(pixel, screen_size);
	
	const int MAX_BOUNCES = 12;
	const float BASE_FIREFLY_THRESHOLD = 0.15;
	
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
			float3 env = throughput * SampleEnvironment(ray.Direction);
			radiance += FireflyFilter(env, BASE_FIREFLY_THRESHOLD, fireflyFilterK);
			break;
		}
		
		Instance instance = GetInstance(hit.instance_id);
		Material material = GetMaterial(instance.material_id);
		SurfaceProperties surface = EvaluateMaterial(material, hit.uv);
		
		// Direct lighting with relaxed firefly threshold
		float3 direct = throughput * SampleDirectLighting(tlas, hit, surface, -ray.Direction, rng);
		radiance += FireflyFilter(direct, BASE_FIREFLY_THRESHOLD * 6.0, fireflyFilterK);
		
		// Sample BRDF and update path
		MaterialSample brdf_sample = SampleBRDF(surface, -ray.Direction, hit.normal, rng);
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
		
		ray.Origin = hit.position + hit.normal * 0.001;
		ray.Direction = brdf_sample.direction;
		ray.TMin = 0.001;
		ray.TMax = 10000.0;
	}
	
	radiance = min(radiance, float3(100, 100, 100));
	AccumulateAndOutput(pixel, radiance);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
	payload.hit = false;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
	payload.hit = true;
	payload.depth = RayTCurrent();
	payload.bary = attribs.barycentrics;
	payload.instance_id = InstanceID();
	payload.primitive_id = PrimitiveIndex();
}
