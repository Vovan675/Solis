#include "common.h"

cbuffer Uniforms : register(b1)
{
    uint equirect_tex_id;
};

RWTexture2DArray<float4> cubemap_image : register(u0);
static Texture2D equirect_texture = ResourceDescriptorHeap[equirect_tex_id];

float2 cartesianToSpherical(float3 cartesian)
{
    float r = length(cartesian);
    float theta = atan2(cartesian.z, cartesian.x);    // Azimuth
    float phi = acos(cartesian.y / r);               // Polar
    return float2(theta / PI2, phi / PI);
}

[numthreads(32, 32, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID)
{
    // Get cubemap face coordinates
    uint3 coord = dispatchID;
    uint3 size;
    cubemap_image.GetDimensions(size.x, size.y, size.z);
    
    if (coord.x >= size.x || coord.y >= size.y) return;
    
    float3 N = GetCubemapNormal(size.xy, dispatchID);
    
    // Convert to spherical coordinates and sample equirectangular texture
    float2 sphericalUV = cartesianToSpherical(N);
    float4 color = equirect_texture.SampleLevel(linear_wrap_sampler, sphericalUV, 0);
    cubemap_image[coord] = color;
}