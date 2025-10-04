#include "../common.h"
#include "../bindless.h"

[[vk::image_format("rgba8")]]
RWTexture2D<float4> image : register(u0);
cbuffer Lights : register(b1) {
    float4 dir_light_direction;
	uint depthTexId;
};

RaytracingAccelerationStructure topLevelAS : register(t2);

struct RayPayload {
    bool hit;
};

[shader("raygeneration")]
void RayGen() {
    uint2 launchId = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;
    
    float2 pixelCenter = launchId + 0.5f;
    float2 inUV = pixelCenter / launchSize;
    float2 d = inUV * 2.0f - 1.0f;
    d.y *= -1.0f;

	float depth = SampleTextureLevel(depthTexId, inUV, 0).r;
	float3 world_pos = GetWSPosition(inUV, depth);

    float3 origin = world_pos;
    float4 direction = dir_light_direction;

    RayPayload payload;
    payload.hit = false;

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.1;
    ray.TMax = 10000.0;

    TraceRay(
        topLevelAS,
        RAY_FLAG_NONE,
        0xff, 0, 0, 0,
        ray, payload
    );

    image[int2(launchId)] = float4(payload.hit ? 0 : 1, 0, 0, 0);
}


[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.hit = false;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    payload.hit = true;
}