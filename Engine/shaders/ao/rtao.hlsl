#include "ao_common.h"
#include "../ray_tracing.h"
#include "../random.h"
#include "../sampling.h"

cbuffer Constants : register(b0)
{
	uint depth_tex_id;
	uint normal_tex_id;
	uint output_tex_id;
	uint accumulation_tex_id;
	uint accumulated_frames;
	int rays_per_pixel;
	float radius;
	float falloff;
	uint sampling_noise_tex_id;
	int resolution_scale;
};

static Texture2D<float> depth_texture = ResourceDescriptorHeap[depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
static Texture2D sampling_noise = ResourceDescriptorHeap[sampling_noise_tex_id];
[[vk::image_format("r8")]]
static RWTexture2D<float> output_texture = ResourceDescriptorHeap[output_tex_id];
#if ACCUMULATE
	[[vk::image_format("r32f")]]
	static RWTexture2D<float> accumulation_texture = ResourceDescriptorHeap[accumulation_tex_id];
#endif

static int2 resolution = getScaledRenderResolution(resolution_scale);

float traceOcclusionRay(RaytracingAccelerationStructure tlas, float3 origin, float3 direction)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0;
	ray.TMax = radius;

	RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, RAY_FLAG_NONE, RAY_TRACING_MASK_SHADOW, ray);
	while (ray_query.Proceed())
	{
		if (!isAlphaClipped(ray_query.CandidateInstanceID(), ray_query.CandidatePrimitiveIndex(), ray_query.CandidateTriangleBarycentrics()))
			ray_query.CommitNonOpaqueTriangleHit();
	}

	if (ray_query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
		return 0.0f;

	return saturate(((radius - ray_query.CommittedRayT()) / (falloff * radius)));
	//return ray_query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(resolution)))
		return;

	float depth = depth_texture[pixel * resolution_scale];
	if (depth == 0.0)
	{
		output_texture[pixel] = 1.0;
		return;
	}

	float3 position = mul(iview, float4(GetVSPosition(getScaledPixelUV(pixel, resolution_scale), depth), 1.0)).xyz;
	float3 normal = unpackGBufferNormal(normal_texture[pixel].rgb);
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];

	float3 tangent, bitangent;
	BuildOrthonormalBasis(normal, tangent, bitangent);

	float visibility = 0;

	float bias = max(0.001f, 0.0001f * length(position - camera_position.xyz));

	RandomState rng = InitRandomState(pixel.x + pixel.y * resolution.x, frame);

	for (int i = 0; i < rays_per_pixel; i++)
	{
		#if ACCUMULATE
			float3 dir_tangent = SampleCosineHemisphere(Random2(rng));
		#else
			// FAST noise for the 5x5 box filter
			uint2 noise_offset = R2(i) * 128;
			float3 dir_tangent = SampleCosineHemisphere(sampleFastNoise(sampling_noise, pixel + noise_offset, TEMPORAL));
		#endif
		float3 dir = TangentToWorld(dir_tangent, tangent, bitangent, normal);
		float occluded = traceOcclusionRay(tlas, position + (normal + dir) * bias, dir);
		visibility += 1.0f - occluded;
	}
	visibility /= rays_per_pixel;

	#if ACCUMULATE
		float history = accumulated_frames == 0 ? 0.0 : accumulation_texture[pixel];
		float mean = lerp(history, visibility, 1.0 / (accumulated_frames + 1));
		accumulation_texture[pixel] = mean;
		output_texture[pixel] = mean;
	#else
		output_texture[pixel] = visibility;
	#endif
}
