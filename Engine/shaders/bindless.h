#pragma once
Texture2D BindlessTextures[] : register(t0, space1);
SamplerState BindlessSamplers[] : register(s0, space1);

float4 SampleTexture(uint index, float2 uv)
{
    return BindlessTextures[NonUniformResourceIndex(index)].Sample(BindlessSamplers[NonUniformResourceIndex(index)], uv);
}

float4 SampleTextureLevel(uint index, float2 uv, int lod)
{
    return BindlessTextures[NonUniformResourceIndex(index)].SampleLevel(BindlessSamplers[NonUniformResourceIndex(index)], uv, 0);
}