#include "../bindless.h"
#include "lighting.h"

struct VSInput
{
	float4 inPos : POSITION;
	float2 inUV : TEXCOORD0;
	float3 inNormal : NORMAL;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 outPos : UV_POS;
};

cbuffer Constants : register(b0)
{
	uint albedo_tex_id;
	uint normal_tex_id;
	uint depth_tex_id;
	uint shading_tex_id;
	uint light_index;
	uint ray_traced_visibility_tex_id;
};

VSOutput VSMain(VSInput input)
{
	Light light = getLight(light_index);
	float3 world_pos;
	if (light.type == LIGHT_TYPE_DIRECTIONAL)
		world_pos = camera_position.xyz + input.inPos.xyz;
	else
		world_pos = light.position.xyz + input.inPos.xyz * light.attenuation_radius;

	VSOutput output;
	output.position = mul(view_projection, float4(world_pos, 1.0));
	output.outPos = output.position;
	return output;
}

struct PSOutput
{
	float3 outDiffuse : SV_Target0;
	float3 outSpecular : SV_Target1;
};

PSOutput PSMain(VSOutput input)
{
	float2 inUV = input.outPos.xy / input.outPos.w * float2(0.5, -0.5) + 0.5;
	float depth = SampleTexture(depth_tex_id, inUV).r;
	float4 shading = SampleTexture(shading_tex_id, inUV);
	float3 albedo = SampleTexture(albedo_tex_id, inUV).rgb;
	float metalness = shading.r;
	float perceptual_roughness = max(saturate(shading.g), MIN_PERCEPTUAL_ROUGHNESS);
	float specular = shading.b;

	float3 P = GetWSPosition(inUV, depth);
	float3 N = unpackGBufferNormal(SampleTexture(normal_tex_id, inUV, point_clamp_sampler).rgb);
	float3 V = normalize(camera_position.xyz - P);

	Light light = getLight(light_index);
	float visibility;
	if (light.type == LIGHT_TYPE_DIRECTIONAL && ray_traced_visibility_tex_id != 0)
		visibility = SampleTexture(ray_traced_visibility_tex_id, inUV).r;
	else
		visibility = getLightShadow(light, P, N, random(inUV));

	PSOutput output;
	calculateDirectLight(light, P, N, V, albedo, metalness, perceptual_roughness, specular, visibility, output.outDiffuse, output.outSpecular);

	//#define SHOW_CASCADES
	#ifdef SHOW_CASCADES
		if (light.type == LIGHT_TYPE_DIRECTIONAL)
		{
			float view_depth = -mul(view, float4(P, 1.0)).z;
			switch (getCascadeIndex(light, view_depth))
			{
				case 0: output.outDiffuse = float3(1, 0, 0); break;
				case 1: output.outDiffuse = float3(0, 1, 0); break;
				case 2: output.outDiffuse = float3(0, 0, 1); break;
				case 3: output.outDiffuse = float3(1, 1, 0); break;
			}
			output.outSpecular = 0;
		}
	#endif
	return output;
}
