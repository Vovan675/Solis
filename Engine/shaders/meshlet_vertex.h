#pragma once
#include "common.h"
#include "bindless.h"
#include "gpu_driven/culling.h"

cbuffer GeometryPassConstants : register(b0)
{
	float4x4 pass_view_projection;
	uint visible_meshlets_buffer_id;
	uint group_residency_buffer_id;
};

static StructuredBuffer<MeshletCandidate> visible_meshlets_buffer = ResourceDescriptorHeap[visible_meshlets_buffer_id];
static ByteAddressBuffer meshlets_geometry_buffer = ResourceDescriptorHeap[global_meshlets_geometry_buffer_id];
static RWByteAddressBuffer group_residency_buffer = ResourceDescriptorHeap[group_residency_buffer_id];

struct MeshletDraw
{
	MeshletCandidate candidate;
	Instance instance;
	Meshlet meshlet;
	Mesh mesh;
	uint geometry_base;
};

MeshletDraw fetchMeshletDraw(uint meshlet_candidate_id)
{
	MeshletDraw draw;
	draw.candidate = visible_meshlets_buffer[meshlet_candidate_id];
	draw.instance = getInstance(draw.candidate.instance_id);
	draw.meshlet = getMeshlet(draw.candidate.meshlet_id, group_residency_buffer_id);
	draw.mesh = getMesh(draw.instance.mesh_id);

	GroupResidency residency = group_residency_buffer.Load<GroupResidency>(
		sizeof(GroupResidency) * (draw.mesh.group_residency_offset + draw.meshlet.group_id));
	draw.geometry_base = uint(residency.geometry_buffer_offset);
	return draw;
}

struct RawVertexData
{
	float3 position;
	float3 normal;
	float4 tangent; // xyz = direction, w = bitangent sign
	float2 uv;
};

RawVertexData loadMeshletVertex(uint local_vertex_id, MeshletDraw draw)
{
	// Vertex layout (stride 24, or 32 with tangent):
	// pos float3 | normal oct-snorm16 | uv float2
	// [ tangent oct-snorm16 | sign int8 | pad 3B ]
	bool has_tangent = (draw.mesh.attribute_flags & MESH_ATTR_TANGENT) != 0;
	uint stride = has_tangent ? 32u : 24u;
	uint base = draw.geometry_base + draw.meshlet.vertex_offset + local_vertex_id * stride;

	RawVertexData vertex;
	vertex.position = asfloat(meshlets_geometry_buffer.Load3(base));
	vertex.normal = octDecode(unpackSnorm16x2(meshlets_geometry_buffer.Load(base + 12)));
	vertex.uv = asfloat(meshlets_geometry_buffer.Load2(base + 16));

	if (has_tangent)
	{
		uint2 packed_tangent = meshlets_geometry_buffer.Load2(base + 24);
		float3 tangent_dir = octDecode(unpackSnorm16x2(packed_tangent.x));
		float sign = (packed_tangent.y & 0xFF) == 1 ? 1.0 : -1.0;
		vertex.tangent = float4(tangent_dir, sign);
	} else
	{
		vertex.tangent = float4(0, 0, 0, 1);
	}

	return vertex;
}
