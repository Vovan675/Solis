#include "../meshlet_vertex.h"

struct VS_OUTPUT
{
	float4 outPos : SV_POSITION;
	float2 uv : TEXCOORD0;
	nointerpolation uint material_id : MATERIAL_ID;
};

VS_OUTPUT shadowOutput(float3 local_position, float2 uv, Instance instance)
{
	VS_OUTPUT output;
	output.outPos = mul(cull_views[view_id].view_projection, mul(instance.world_transform, float4(local_position, 1.0)));
	output.uv = uv;
	output.material_id = instance.material_id;
	return output;
}

[NumThreads(32, 1, 1)]
[OutputTopology("triangle")]
void MSMainMeshlet(uint group_index : SV_GroupIndex, uint group_id : SV_GroupID,
			out vertices VS_OUTPUT verts[128], out indices uint3 indices[128])
{
	MeshletDraw draw = fetchMeshletDraw(group_id);
	uint vertex_count = draw.meshlet.getVertexCount();
	uint triangle_count = draw.meshlet.getTriangleCount();
	SetMeshOutputCounts(vertex_count, triangle_count);

	for (uint i = group_index; i < vertex_count; i += 32)
	{
		RawVertexData raw_vertex = loadMeshletVertex(i, draw);
		verts[i] = shadowOutput(raw_vertex.position, raw_vertex.uv, draw.instance);
	}

	for (uint j = group_index; j < triangle_count; j += 32)
		indices[j] = loadMeshletTriangle(j, draw);
}

VS_OUTPUT VSMainMeshlet(uint instance_id : INSTANCE_ID, uint vertex_id : SV_VertexID)
{
	MeshletDraw draw = fetchMeshletDraw(instance_id);
	RawVertexData raw_vertex = loadMeshletVertex(loadMeshletIndex(vertex_id, draw), draw);
	return shadowOutput(raw_vertex.position, raw_vertex.uv, draw.instance);
}

VS_OUTPUT VSMainTraditional(uint instance_id : INSTANCE_ID, uint vertex_id : SV_VertexID)
{
	Instance instance = getInstance(instance_id);
	Mesh mesh = getMesh(instance.mesh_id);
	uint index = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, vertex_id, sizeof(uint));
	float3 position = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index, mesh.vertex_stride);
	float2 uv = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index, mesh.vertex_stride);
	return shadowOutput(position, uv, instance);
}

void PSMain(VS_OUTPUT input)
{
	Material material = getMaterial(input.material_id);
	if (material.alpha_tex_id != 0 && material.albedo.a * SampleTexture(material.alpha_tex_id, input.uv).a < material.alpha_cutoff)
		discard;
}
