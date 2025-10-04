#include "common.h"

RWTexture2DArray<float4> cubemapImage : register(u0);
Texture2D<float4> equirectangularTexture : register(t1);
SamplerState equirectSampler : register(s1);

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
    cubemapImage.GetDimensions(size.x, size.y, size.z);
    
    if (coord.x >= size.x || coord.y >= size.y) return;
    
    float3 N = GetCubemapNormal(size.xy, dispatchID);
    
    // Convert to spherical coordinates and sample equirectangular texture
    float2 sphericalUV = cartesianToSpherical(N);
    float4 color = equirectangularTexture.SampleLevel(equirectSampler, sphericalUV, 0);
    cubemapImage[coord] = color;
}