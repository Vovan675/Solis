#include "../common.h"

[[vk::image_format("rgba8")]]
RWTexture2D<float4> image : register(u0);
cbuffer Lights : register(b1) {
    float4 dir_light_direction;
};

RaytracingAccelerationStructure topLevelAS : register(t2);

struct RayPayload {
    float3 color;
    float t;
    float3 normal;
};

float2 Hammersley(uint i, uint N) {
    float phi = float(i) * 6.283185 / float(N);
    float r = sqrt(float(i)/float(N));
    return float2(r*cos(phi), r*sin(phi));
}

float2x2 randomRotation(float2 p) {
    float angle = frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453123) * 6.283185;
    return float2x2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

[shader("raygeneration")]
void RayGen() {
    uint2 launchId = DispatchRaysIndex().xy;
    uint2 launchSize = DispatchRaysDimensions().xy;
    
    float2 pixelCenter = launchId + 0.5f;
    float2 inUV = pixelCenter / launchSize;
    float2 d = inUV * 2.0f - 1.0f;
    d.y *= -1.0f;

    float4 origin = mul(iview, float4(0, 0, 0, 1));
    float4 target = mul(iprojection, float4(d.x, d.y, 1, 1));
    float4 direction = mul(iview, float4(normalize(target.xyz), 0));

    RayPayload payload;
    payload.color = 0;
    payload.t = 0;

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;
    ray.TMin = 0.001;
    ray.TMax = 10000.0;

    TraceRay(
        topLevelAS,
        RAY_FLAG_NONE,
        0xff, 0, 0, 0,
        ray, payload
    );

    RayPayload prev_payload = payload;
    float3 light_dir = normalize(dir_light_direction.xyz);

    if (any(prev_payload.color != 0)) {
        float3 shadow_origin = origin.xyz + direction.xyz * payload.t;
        int light_samples = 1;
        float shadow_factor = 0;

        float2x2 rotate = randomRotation(inUV);

        [unroll]
        for (uint i = 0; i < light_samples; i++) {
            float2 offset = mul(rotate, Hammersley(i+1, light_samples+1)) * tan(radians(1.5)*0.5);
            float3 shadow_dir = normalize(light_dir + float3(offset.x, offset.y, 0));

            payload.color = 0;
            payload.t = 0;
            
            RayDesc shadow_ray;
            shadow_ray.Origin = shadow_origin.xyz;
            shadow_ray.Direction = shadow_dir.xyz;
            shadow_ray.TMin = 0.001;
            shadow_ray.TMax = 10000.0;

            TraceRay(
                topLevelAS,
                RAY_FLAG_NONE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
                0xff, 0, 0, 0,
                shadow_ray, payload
            );

            if (all(payload.color == 0))
                shadow_factor += 1.0f;
        }
        shadow_factor /= light_samples;
        prev_payload.color *= shadow_factor;
    }

    image[int2(launchId)] = float4(prev_payload.color, 0);
}



[shader("miss")]
void Miss(inout RayPayload payload) {
    payload.color = 0;
}



/*
struct ObjDesc {
    float4 color;
    uint vertexBufferOffset;
    uint indexBufferOffset;
};
StructuredBuffer<ObjDesc> obj_descs : register(t3);

struct Vertex {
    float3 pos;
    float3 normal;
    float3 tangent;
    float2 uv;
    float3 color;
};
StructuredBuffer<Vertex> vertices : register(t4);
StructuredBuffer<uint> indices : register(t5);

AttributePayload float2 attribs;
*/

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs) {
    payload.color = 1;

    /*
    uint instanceIndex = GetInstanceIndex();
    ObjDesc obj_desc = obj_descs[instanceIndex];
    
    uint index[3] = {
        indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3],
        indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3 + 1],
        indices[obj_desc.indexBufferOffset + gl_PrimitiveID * 3 + 2]
    };

    Vertex v[3] = {
        vertices[obj_desc.vertexBufferOffset + index[0]],
        vertices[obj_desc.vertexBufferOffset + index[1]],
        vertices[obj_desc.vertexBufferOffset + index[2]]
    };

    float3 barycentric = float3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    float3 pos = v[0].pos*barycentric.x + v[1].pos*barycentric.y + v[2].pos*barycentric.z;
    float3 world_pos = mul((float3x3)ObjectToWorld, pos) + ObjectToWorld[3].xyz;

    float3 normal = v[0].normal*barycentric.x + v[1].normal*barycentric.y + v[2].normal*barycentric.z;
    float3 world_normal = normalize(mul((float3x3)ObjectToWorld, normal));

    float3 light_dir = normalize(dir_light.dir_light_direction.xyz);
    float lighting = dot(world_normal, light_dir);
    
    payload.color = float3(lighting, lighting, lighting);
    payload.normal = world_normal;
    payload.t = CurrentRayT();
    */
}