cbuffer PerFrameConstants : register(b32)
{
	matrix view;
	matrix projection;
};

cbuffer PerDrawCallConstants : register(b0)
{
	matrix model;
	float3x3 imodel;
};

#ifdef VULKAN
//[[vk::push_constant]]
#endif
//ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

SamplerState MySampler1 : register(s1);
Texture2D MyTexture1 : register(t1);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
};

PSInput VSMain(VertexInput IN)
{
    PSInput result;

	matrix mvp = mul(projection, mul(view, model));
	result.position = mul(mvp, float4(IN.position, 1.0));
	result.normal = mul(float3x3(imodel), IN.normal);
	//result.normal = normal;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
	float3 res = normalize(input.normal);
	float light = dot(res, normalize(float3(1, -1, 2)));
	//return float4((res + 1.0) / 2.0f, 1);
	return float4(light, light, light, 1);
	//return float4(res, 1);
	/*
	return textures[0].Sample(MySampler1, float2(input.uv)) * ModelViewProjectionCB.MVP._11.r;
	if (input.uv.x > 0.5)
	{
		//return 1;
		return MyTexture1.Sample(MySampler1, float2(input.uv)); // left
	}
	else
	{
		return MyTexture2.Sample(MySampler2, float2(input.uv));
	}
*/
	//return MyTexture1.Sample(MySampler1, float2(input.uv)) ;
	//return MyTexture2.Sample(MySampler2, float2(input.uv));
    //return input.color;
}
