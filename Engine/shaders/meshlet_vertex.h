#pragma once
#include "common.h"
#include "bindless.h"
#include "gpu_driven/culling.h"

cbuffer GeometryPassConstants : register(b0)
{
	uint view_id;
	uint cull_views_buffer_id;
	uint visible_meshlets_buffer_id;
	uint group_residency_buffer_id;
};

static StructuredBuffer<CullView> cull_views = ResourceDescriptorHeap[cull_views_buffer_id];
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

// Low 32 bits of 64 bit number (high:low) >> shift.
uint shiftRight64(uint high, uint low, uint shift)
{
	return shift == 0 ? low : (low >> shift) | (high << (32 - shift));
}

// Lowest bit_count bits of value
uint lowBits(uint value, uint bit_count)
{
	return value & ((1u << bit_count) - 1);
}

uint3 loadMeshletPositionOffset(uint positions_offset, uint bit_offset, uint3 position_bits)
{
	uint3 data = meshlets_geometry_buffer.Load3(positions_offset + ((bit_offset >> 5) << 2));
	uint low = shiftRight64(data.y, data.x, bit_offset & 31);
	uint high = shiftRight64(data.z, data.y, bit_offset & 31);

	// Extract value by value and shift low and high to the next value
	uint3 offset;
	offset.x = lowBits(low, position_bits.x);

	low = shiftRight64(high, low, position_bits.x);
	high >>= position_bits.x;

	offset.y = lowBits(low, position_bits.y);

	low = shiftRight64(high, low, position_bits.y);

	offset.z = lowBits(low, position_bits.z);
	return offset;
}

RawVertexData loadMeshletVertex(uint local_vertex_id, MeshletDraw draw)
{
	// Vertex layout: positions (bit sequence, up to 48 bits each) padded to 4 bytes, then attributes:
	// normal oct-snorm16 | [ tangent oct-snorm16 | sign int8 | pad 3B ] | [ uv float2 ]

	bool has_tangent = (draw.mesh.attribute_flags & MESH_ATTR_TANGENT) != 0;
	bool has_uv = (draw.mesh.attribute_flags & MESH_ATTR_UV) != 0;

	uint uv_offset = has_tangent ? 12u : 4u;
	uint attribute_stride = has_uv ? uv_offset + 8u : uv_offset;

	uint3 position_bits = draw.meshlet.getPositionBits();
	uint vertex_bits = position_bits.x + position_bits.y + position_bits.z;

	uint positions_offset = draw.geometry_base + draw.meshlet.vertex_offset;
	uint positions_size = divideRoundUp(draw.meshlet.getVertexCount() * vertex_bits, 32) * 4;
	uint attributes_offset = positions_offset + positions_size + local_vertex_id * attribute_stride;

	RawVertexData vertex;

	uint position_bit_offset = local_vertex_id * vertex_bits;
	uint3 position_offset = loadMeshletPositionOffset(positions_offset, position_bit_offset, position_bits);
	vertex.position = draw.meshlet.bound_min + float3(position_offset) * draw.mesh.position_scale;

	vertex.normal =octDecode(unpackSnorm16x2(meshlets_geometry_buffer.Load(attributes_offset)));

	if (has_tangent)
	{
		uint2 packed_tangent = meshlets_geometry_buffer.Load2(attributes_offset + 4);
		float3 tangent_dir = octDecode(unpackSnorm16x2(packed_tangent.x));
		float sign = (packed_tangent.y & 0xFF) == 1 ? 1.0 : -1.0;
		vertex.tangent = float4(tangent_dir, sign);
	} else
	{
		vertex.tangent = float4(0, 0, 0, 1);
	}

	vertex.uv = has_uv ? asfloat(meshlets_geometry_buffer.Load2(attributes_offset + uv_offset)) : float2(0, 0);

	return vertex;
}
