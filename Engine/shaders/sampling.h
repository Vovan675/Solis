
#pragma once
#include "common.h"

// From pbr book
float2 SampleUniformDiskConcentric(float2 u)
{
    float2 uOffset = 2 * u - float2(1, 1);
    if (uOffset.x == 0 && uOffset.y == 0)
        return float2(0, 0);

    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y)) {
        r = uOffset.x;
        theta = PI/4 * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = PI/2 - PI/4 * (uOffset.x / uOffset.y);
    }
    return r * float2(cos(theta), sin(theta));
}

float3 SampleCosineHemisphere(float2 u) {
    float2 d = SampleUniformDiskConcentric(u);
    float z = sqrt(max(Epsilon, 1 - d.x * d.x - d.y * d.y));
    return float3(d.x, d.y, z);
}

float3 UniformSampleHemisphere(float2 u) {
    float z = u[0];
    float r = sqrt(max(0, 1. - z * z));
    float phi = 2 * PI * u[1];
    return float3(r * cos(phi), r * sin(phi), z);
}