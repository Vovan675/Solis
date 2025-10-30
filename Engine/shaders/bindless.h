#pragma once
#include "common.h"

#ifndef RAY_TRACING_SHADER
float4 SampleTexture(uint index, float2 uv)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.Sample(linear_wrap_sampler, uv);
}

float4 SampleTexture(uint index, float2 uv, SamplerState sampler)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.Sample(sampler, uv);
}
#endif

float4 SampleTextureLevel(uint index, float2 uv, int lod)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.SampleLevel(linear_wrap_sampler, uv, 0);
}