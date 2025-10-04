#include "../bindless.h"

struct ModelViewProjection
{
	matrix MVP;
    uint texture_index;
    uint texture2_index;
};

#ifdef VULKAN
//[[vk::push_constant]]
#endif
ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : UV;
};

PSInput VSMain(float3 position : POSITION, float2 uv : UV)
{
    PSInput result;

	result.position = mul(ModelViewProjectionCB.MVP, float4(position, 1.0));
	result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	if (input.uv.x > 0.5)
	{
        return SampleTexture(ModelViewProjectionCB.texture_index, input.uv);
	}
	else
	{
        return SampleTexture(ModelViewProjectionCB.texture2_index, input.uv);
	}
}
