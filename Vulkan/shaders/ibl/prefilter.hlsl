#include "../common.h"

RWTexture2DArray<float4> prefilterMap : register(u1);
TextureCube texSampler : register(t2);
SamplerState texSamplerState : register(s2);

cbuffer Constants : register(b0)
{
    float roughness;
}

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}  

float D_GGX(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom); 
}

float3 prefilterEnvMap(float3 R, float roughness)
{
    const float3 N = normalize(R);
    float3 V = R;
    const uint SAMPLES_COUNT = 32u;
    float total_weight = 0.0;
    float3 prefiltered_color = float3(0, 0, 0);
    
    for (uint i = 0u; i < SAMPLES_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLES_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = saturate(dot(N, L));
        if (NdotL > 0.0)
        {
            float NdotH = saturate(dot(N, H));
            float VdotH = saturate(dot(V, H));
            float pdf = D_GGX(NdotH, roughness) * NdotH / (4.0 * VdotH) + 0.0001;
            
            float width, height;
            texSampler.GetDimensions(width, height);
            float resolution = width;
            float omega_s = 1.0 / (float(SAMPLES_COUNT) * pdf);
            float omega_p = 4.0 * PI / (6.0 * resolution * resolution);
            float mip_level = roughness == 0.0 ? 0.0 : max(0.5 * log2(omega_s / omega_p) + 1.0, 0.0);
            
            prefiltered_color += texSampler.SampleLevel(texSamplerState, L, mip_level).rgb * NdotL;
            total_weight += NdotL;
        }
    }
    
    return prefiltered_color / total_weight;
}

[numthreads(32, 32, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint3 size;
    prefilterMap.GetDimensions(size.x, size.y, size.z);
    if (id.x >= size.x || id.y >= size.y) return;
    
    float3 N = GetCubemapNormal(float2(size.x, size.y), id);
    float3 color = prefilterEnvMap(N, roughness);
    prefilterMap[id] = float4(color, 1);
}