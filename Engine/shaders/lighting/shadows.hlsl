#include "../common.h"

struct VS_INPUT
{
	uint instance_id : INSTANCE_ID;
	uint vertex_id : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 outPos : SV_POSITION;
	float4 worldPos : TEXCOORD0;
};

cbuffer UBO : register(b1)
{
	float4x4 light_space_matrix;
	float4 light_pos;
	float shadow_z_far;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	Instance instance = getInstance(input.instance_id);
	Mesh mesh = getMesh(instance.mesh_id);
	float3 vertex_pos = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, input.vertex_id, mesh.vertex_stride);

	VS_OUTPUT output;
	output.outPos = mul(light_space_matrix, mul(instance.world_transform, float4(vertex_pos, 1.0)));
	output.worldPos = mul(instance.world_transform, float4(vertex_pos, 1.0));
	return output;
}

#if LIGHT_TYPE == 0
	float PSMain(VS_OUTPUT input) : SV_Depth
	{
		float lightDistance = length(input.worldPos.xyz - light_pos.xyz);
		return lightDistance / shadow_z_far;
	}
#elif LIGHT_TYPE == 1
	void PSMain(VS_OUTPUT input)
	{
	}
#endif
