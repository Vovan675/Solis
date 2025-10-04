#include "../common.h"

RWTexture2DArray<float4> irradianceMap : register(u1);
TextureCube texSampler : register(t2);
SamplerState texSamplerState : register(s2);

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N) {
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 sampleHemisphere(float u1, float u2) {
    float u1p = sqrt(max(0.0, 1.0 - u1 * u1));
    return float3(cos(PI2 * u2) * u1p, sin(PI2 * u2) * u1p, u1);
}

[numthreads(32, 32, 1)]
void CSMain(uint3 dispatchID : SV_DispatchThreadID) {
    uint3 size;
    irradianceMap.GetDimensions(size.x, size.y, size.z);
    
    float3 N = GetCubemapNormal(size.xy, dispatchID);
    float3 up, right;
    ComputeBasis(N, up, right);

    float3 irradiance = float3(0.0, 0.0, 0.0);
    int samples_count = 4096;

    for (int i = 0; i < samples_count; i++) {
        float2 Xi = Hammersley(i, samples_count);
        float3 sample_tangent = sampleHemisphere(Xi.x, Xi.y);
        float3 sample_dir = sample_tangent.x * right + sample_tangent.y * up + sample_tangent.z * N;
        float cosTheta = max(0.0, dot(N, sample_dir));
        irradiance += texSampler.SampleLevel(texSamplerState, sample_dir, 0).rgb * cosTheta;
    }
    
    irradiance = PI * irradiance / float(samples_count);
    
    irradianceMap[dispatchID] = float4(irradiance, 1.0);
}
