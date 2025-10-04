struct ModelViewProjection
{
    matrix MVP;
};

#ifdef VULKAN
//[[vk::push_constant]]
#endif
ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

SamplerState MySampler1 : register(s1);
Texture2D MyTexture1 : register(t1);

SamplerState MySampler2 : register(s2);
Texture2D MyTexture2 : register(t2);

//Texture2D textures[] : register(t0, space1);

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
	//return textures[0].Sample(MySampler1, float2(input.uv)) * ModelViewProjectionCB.MVP._11.r;
	if (input.uv.x > 0.5)
	{
		//return 1;
		return MyTexture1.Sample(MySampler1, float2(input.uv)); // left
	}
	else
	{
		return MyTexture2.Sample(MySampler2, float2(input.uv));
	}
	//return MyTexture1.Sample(MySampler1, float2(input.uv)) ;
	//return MyTexture2.Sample(MySampler2, float2(input.uv));
    //return input.color;
	}
