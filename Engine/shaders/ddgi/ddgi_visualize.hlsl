#include "../common.h"
#include "ddgi_common.hlsl"

static DDGIVolume volume = ddgi_volume;

cbuffer Settings : register(b1)
{
	int mode;
};

struct VertexInput {
	float3 position : POSITION;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float2 uv : TEXCOORD0;
	uint instance_id : SV_InstanceID;
};

struct VertexOutput {
	float4 position : SV_POSITION;
	float3 direction : TEXCOORD0;
	uint probe_id : TEXCOORD1;
	uint probe_cascade : TEXCOORD2;
};

VertexOutput VSMain(VertexInput IN) {
	VertexOutput OUT;

	uint cascade = GetProbeCascade(volume, IN.instance_id);
	uint3 probe_coords = GetProbeGridCoords(volume, IN.instance_id);
	float3 probe_position = GetProbeWorldPosition(volume, probe_coords, cascade);
	OUT.position = mul(view_projection_unjittered, float4(IN.position * 0.1 + probe_position, 1.0));
	OUT.direction = IN.position;
	OUT.probe_id = IN.instance_id;
	OUT.probe_cascade = cascade;
	return OUT;
}

float4 PSMain(VertexOutput IN) : SV_TARGET {
	Texture2DArray distance_atlas = ResourceDescriptorHeap[ddgi_distance_atlas_tex_id];
	Texture2DArray irradiance_atlas = ResourceDescriptorHeap[ddgi_irradiance_atlas_tex_id];
	uint3 probe_coords = GetProbeGridCoords(volume, IN.probe_id);

	if (mode == 0)
	{
		float2 uv = GetProbeUV(volume, probe_coords, normalize(IN.direction), IRRADIANCE_TEXELS);
		float3 encoded_irradiance = irradiance_atlas.Sample(linear_clamp_sampler, float3(uv, IN.probe_cascade)).rgb;
		float3 irradiance = encoded_irradiance * GetProbeLightingScale(volume, probe_coords, IN.probe_cascade) * PI2 / PI;
		return LinearToSRGB(float4(irradiance * camera_exposure, 1.0));
	} else if (mode == 1)
	{
		float2 uv = GetProbeUV(volume, probe_coords, normalize(IN.direction), DISTANCE_TEXELS);
		float2 distance = distance_atlas.Sample(linear_clamp_sampler, float3(uv, IN.probe_cascade)).rg;
		return float4(distance.rrr / 20.0f, 1.0);
	} else if (mode == 2 || mode == 3)
	{
		uint state = GetProbeState(volume, probe_coords, IN.probe_cascade);
		
		if (mode == 3 && state == STATE_DISABLED)
		{
			discard;
		}

		if (state == STATE_ENABLED)
			return float4(0.0, 1.0, 0.0, 1.0);
		else if (state == STATE_DISABLED)
			return float4(1.0, 0.0, 0.0, 1.0);
		else
			return float4(1.0, 1.0, 1.0, 1.0);
	} else if (mode == 4)
	{
		float3 cascade_color;
		switch (IN.probe_cascade)
		{
			case 0: cascade_color = float3(1.00, 0.16, 0.24); break;
			case 1: cascade_color = float3(1.00, 0.80, 0.10); break;
			case 2: cascade_color = float3(0.10, 0.85, 0.95); break;
			case 3: cascade_color = float3(0.52, 0.36, 1.00); break;
			default: cascade_color = float3(0.80, 0.80, 0.80); break;
		}
		float shading = 0.7 + 0.3 * saturate(dot(normalize(IN.direction), normalize(float3(0.4, 0.8, 0.3))));
		return float4(cascade_color * shading, 1.0);
	} else if (mode == 5)
	{
		float3 offset = GetProbeWorldPosition(volume, probe_coords, IN.probe_cascade) - GetProbeWorldBasePosition(volume, probe_coords, IN.probe_cascade);
		float offset_length = length(offset / volume.cascades[IN.probe_cascade].spacing.xyz);
		return float4(lerp(float3(0, 1, 0), float3(1, 0, 0), saturate(offset_length / RELOCATION_MAX_OFFSET)), 1.0);
	}
	return float4(1.0, 1.0, 1.0, 1.0);
}