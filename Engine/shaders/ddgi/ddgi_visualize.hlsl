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
	float3 color : COLOR;
	uint instance_id : SV_InstanceID;
};

struct VertexOutput {
	float4 position : SV_POSITION;
	float3 direction : TEXCOORD0;
	uint probe_id : TEXCOORD1;
};

VertexOutput VSMain(VertexInput IN) {
	VertexOutput OUT;

	uint3 probe_coords = GetProbeCoords(volume, IN.instance_id);
	float3 probe_position = GetProbeWorldPosition(volume, probe_coords);
	OUT.position = mul(projection, mul(view, float4(IN.position * 0.1 + probe_position, 1.0)));
	OUT.direction = IN.position;
	OUT.probe_id = IN.instance_id;
	return OUT;
}

float4 PSMain(VertexOutput IN) : SV_TARGET {
	Texture2D distance_atlas = ResourceDescriptorHeap[volume.distance_atlas_tex_id];
	Texture2D irradiance_atlas = ResourceDescriptorHeap[volume.irradiance_atlas_tex_id];
	uint3 probe_coords = GetProbeCoords(volume, IN.probe_id);

	if (mode == 0)
	{
		float2 uv = GetProbeUV(volume, probe_coords, normalize(IN.direction), IRRADIANCE_TEXELS);
		float4 irradiance = irradiance_atlas.Sample(linear_clamp_sampler, uv);
		return float4(irradiance.rgb, 1.0);
	} else if (mode == 1)
	{
		float2 uv = GetProbeUV(volume, probe_coords, normalize(IN.direction), DISTANCE_TEXELS);
		float2 distance = distance_atlas.Sample(linear_clamp_sampler, uv).rg;
		return float4(distance.rrr / 200.0f, 1.0);
	} else if (mode == 2 || mode == 3)
	{
		uint state = GetProbeState(volume, probe_coords);
		
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
	}
	return float4(1.0, 1.0, 1.0, 1.0);
}