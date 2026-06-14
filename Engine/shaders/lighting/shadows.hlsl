#include "../meshlet_vertex.h"

struct VS_OUTPUT
{
	float4 outPos : SV_POSITION;
};

VS_OUTPUT shadowOutput(float3 local_position, float4x4 world_transform)
{
	VS_OUTPUT output;
	output.outPos = mul(pass_view_projection, mul(world_transform, float4(local_position, 1.0)));
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
		verts[i] = shadowOutput(raw_vertex.position, draw.instance.world_transform);
	}

	uint triangle_data_offset = draw.geometry_base + draw.meshlet.triangle_offset;
	for (uint j = group_index; j < triangle_count; j += 32)
	{
		uint triangle_offset = j * 3;
		uint3 tri;
		tri.x = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 0));
		tri.y = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 1));
		tri.z = meshlets_geometry_buffer.Load<uint>(triangle_data_offset + sizeof(uint) * (triangle_offset + 2));
		indices[j] = tri;
	}
}

VS_OUTPUT VSMainMeshlet(uint instance_id : INSTANCE_ID, uint local_vertex_id : SV_VertexID)
{
	MeshletDraw draw = fetchMeshletDraw(instance_id);
	RawVertexData raw_vertex = loadMeshletVertex(local_vertex_id, draw);
	return shadowOutput(raw_vertex.position, draw.instance.world_transform);
}

VS_OUTPUT VSMainTraditional(uint instance_id : INSTANCE_ID, uint vertex_id : SV_VertexID)
{
	Instance instance = getInstance(instance_id);
	Mesh mesh = getMesh(instance.mesh_id);
	uint index = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, vertex_id, sizeof(uint));
	float3 position = GetMeshVertexData<float3>(mesh.vertex_buffer_id, mesh.positions_offset, index, mesh.vertex_stride);
	return shadowOutput(position, instance.world_transform);
}

void PSMain(VS_OUTPUT input) {}
