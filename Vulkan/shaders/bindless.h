#pragma once
Texture2D BindlessTextures[] : register(t0, space1);
SamplerState BindlessSamplers[] : register(s0, space1);

float4 SampleTexture(uint index, float2 uv)
{
    return BindlessTextures[NonUniformResourceIndex(index)].Sample(BindlessSamplers[NonUniformResourceIndex(index)], uv);
}