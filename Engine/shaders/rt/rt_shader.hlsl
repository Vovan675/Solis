#include "../common.h"
#include "../bindless.h"
#include "../lighting/lighting.h"

cbuffer Constants : register(b1)
{
	uint light_index;
	uint depth_texture_id;
	uint output_texture_id;
};

struct RayPayload {
	bool hit;
};

[shader("raygeneration")]
void RayGen() {
	uint2 launchId = DispatchRaysIndex().xy;
	uint2 launchSize = DispatchRaysDimensions().xy;

	float2 pixelCenter = launchId + 0.5f;
	float2 inUV = pixelCenter / launchSize;

	RWTexture2D<float4> output = ResourceDescriptorHeap[output_texture_id];

	Texture2D<float> depth_texture = ResourceDescriptorHeap[depth_texture_id];
	float depth = depth_texture.Load(int3(launchId, 0)).r;
	float3 world_pos = GetWSPosition(inUV, depth);

	Light light = getLight(light_index);
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];

	RayPayload payload;
	payload.hit = false;

	RayDesc ray;
	ray.Origin = world_pos;
	ray.Direction = normalize(light.direction.xyz);
	ray.TMin = 0.005;
	ray.TMax = 10000.0;

	TraceRay(tlas, RAY_FLAG_NONE, 0xff, 0, 0, 0, ray, payload);

	output[int2(launchId)] = float4(payload.hit ? 0 : 1, 0, 0, 0);
}

[shader("miss")]
void Miss(inout RayPayload payload) {
	payload.hit = false;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
	payload.hit = true;
}
