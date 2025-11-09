#include "../common.h"

static const float3 QUAD[6] =
{
	float3(-1.0, -1.0, 0.0),
    float3(-1.0, 1.0, 0.0),
    float3(1.0, -1.0, 0.0),
    float3(-1.0, 1.0, 0.0),
    float3(1.0, 1.0, 0.0),
    float3(1.0, -1.0, 0.0)
};

struct VSOutput
{
	float2 uv : TEXCOORD0;
	float4 position : SV_POSITION;
};

VSOutput VSMain(uint vertexID : SV_VertexID)
{
	VSOutput output;

    // Get the vertex position from the QUAD array
	float3 fragPos = QUAD[vertexID];

    // Calculate the UV coordinates
	output.uv = (fragPos * 0.5 + 0.5).xy;
	output.uv.y = 1.0f - output.uv.y;

    // Set the vertex position
	output.position = float4(fragPos, 1.0);

	return output;
}

cbuffer Constants
{
    uint texture_index;
};

float4 PSMain(VSOutput input) : SV_TARGET
{
	Texture2D texture = ResourceDescriptorHeap[texture_index];

	// Blur
	float3 sum = float3(0, 0, 0);
	
	float count = 0.0f;
	
	uint width, height;
	texture.GetDimensions(width, height);

	float2 texel_size = float2(1.0f / width, 1.0f / height);
	
	for (int x = -3; x <= 3; x++)
	{
		for (int y = -3; y <= 3; y++)
		{
			float2 offset = float2(x * texel_size.x, y * texel_size.y);
			sum += texture.Sample(linear_wrap_sampler, input.uv + offset).rgb;
			count++;
		}
	}
	
	return float4(sum / count, 1.0f);
	return float4(input.uv.xy, 0, 1);
}