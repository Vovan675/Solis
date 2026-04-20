#include "../common.h"
#include "ddgi_common.hlsl"

static StructuredBuffer<DDGIVolume> volumes = ResourceDescriptorHeap[ddgi_volume_buffer_id];
static DDGIVolume volume = volumes[0];

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
	OUT.position = mul(projection, mul(view, float4(IN.position * 0.1 + probe_position, 1.0)));
	OUT.direction = IN.position;
	OUT.probe_id = IN.instance_id;
	OUT.probe_cascade = cascade;

	//if (cascade != 0) OUT.position = 0;
	return OUT;
}

float4 PSMain(VertexOutput IN) : SV_TARGET {
	Texture2DArray distance_atlas = ResourceDescriptorHeap[volume.distance_atlas_tex_id];
	Texture2DArray irradiance_atlas = ResourceDescriptorHeap[volume.irradiance_atlas_tex_id];
	uint3 probe_coords = GetProbeGridCoords(volume, IN.probe_id);

	//#define SHOW_CASCADES
	#ifdef SHOW_CASCADES
		
	#endif
	//return float4(probe_coords / 32.0, 1.0);

	if (mode == 0)
	{
		float2 uv = GetProbeUV(volume, probe_coords, normalize(IN.direction), IRRADIANCE_TEXELS);
		float4 irradiance = irradiance_atlas.Sample(linear_clamp_sampler, float3(uv, IN.probe_cascade));
		return float4(irradiance.rgb, 1.0);
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
			case 0: cascade_color = float3(1, 0, 0); break;
			case 1: cascade_color = float3(0, 1, 0); break;
			case 2: cascade_color = float3(0, 0, 1); break;
			case 3: cascade_color = float3(1, 1, 0); break;
			default: cascade_color = float3(0, 1, 1); break;
		}
		return float4(cascade_color, 1.0);
	}
	return float4(1.0, 1.0, 1.0, 1.0);
}