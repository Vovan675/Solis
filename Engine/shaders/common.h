#pragma once
#include "hash.h"
// Constants
#define PI 3.14159265359
#define PI2 (2 * PI)
#define Epsilon 0.00001

float random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float2x2 get2DRotationMatrix(float aSin, float aCos)
{
    return float2x2(
        aCos, -aSin,
        aSin, aCos
    );
}

// Camera and Frame Constants
cbuffer CameraConstants : register(b32) {
    float4x4 view;
    float4x4 iview;
    float4x4 projection;
    float4x4 iprojection;
    float4 camera_position;
    float4 swapchain_size; // (width, height, 1/width, 1/height)
    float time;
    int frame;
};

// Convert viewport coordinates to view space position
float3 GetVSPosition(float2 uv, float hardware_depth) {
    uv.y = 1.0f - uv.y;
    float4 clipPos = float4(uv * 2.0 - 1.0, hardware_depth, 1.0);
    float4 viewPos = mul(iprojection, clipPos);
    viewPos.xyz /= viewPos.w;
    return viewPos.xyz;
}

float3 GetWSPosition(float2 uv, float hardware_depth) {
    uv.y = 1.0f - uv.y;
    float4 clipPos = float4(uv * 2.0 - 1.0, hardware_depth, 1.0);
    float4 worldPos = mul(mul(iview, iprojection), clipPos);
    worldPos.xyz /= worldPos.w;
    return worldPos.xyz;
}

// Convert hardware depth to linear depth
float NativeDepthToLinear(float2 uv, float hardware_depth) {
    return length(GetVSPosition(uv, hardware_depth));
}

// Compute tangent and bitangent from normal
void ComputeBasis(float3 N, out float3 T, out float3 B) {
    T = cross(N, float3(0, 1, 0));
    if (dot(T, T) < Epsilon) {
        T = cross(N, float3(1, 0, 0));
    }
    T = normalize(T);
    B = normalize(cross(N, T));
}

// Converts cube face coordinates to a direction vector in world space
float3 GetCubemapNormal(float2 resolution, uint3 globalID) {
    float2 st = globalID.xy / resolution;
    float2 uv = 2.0 * float2(st.x, 1.0 - st.y) - 1.0;

    float3 normal = float3(0.0, 0.0, 0.0);

    // Determine the normal based on the face
    switch (globalID.z) {
        case 0:  // +X face
            normal = float3(1.0, uv.y, -uv.x);
            break;
        case 1:  // -X face
            normal = float3(-1.0, uv.y, uv.x);
            break;
        case 2:  // +Y face
            normal = float3(uv.x, 1.0, -uv.y);
            break;
        case 3:  // -Y face
            normal = float3(uv.x, -1.0, uv.y);
            break;
        case 4:  // +Z face
            normal = float3(uv.x, uv.y, 1.0);
            break;
        case 5:  // -Z face
            normal = float3(-uv.x, uv.y, -1.0);
            break;
    }

    return normalize(normal);
}

// Color space conversions
float4 SRGBToLinear(float4 srgb) {
    return float4(pow(srgb.rgb, 2.2), srgb.a);
}

float4 LinearToSRGB(float4 l) {
    return float4(pow(l.rgb, 1.0/2.2), l.a);
}