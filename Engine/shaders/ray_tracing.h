#pragma once
#include "common.h"

bool isAlphaClipped(uint instance_id, uint primitive_index, float2 barycentrics)
{
	Instance instance = getInstance(instance_id);
	Material material = getMaterial(instance.material_id);
	if (material.alpha_tex_id == 0)
		return false;

	Mesh mesh = getMesh(instance.mesh_id);
	uint index_0 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_index, sizeof(uint));
	uint index_1 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_index + 1, sizeof(uint));
	uint index_2 = GetMeshVertexData<uint>(mesh.index_buffer_id, 0, 3 * primitive_index + 2, sizeof(uint));

	float2 uv_0 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_0, mesh.vertex_stride);
	float2 uv_1 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_1, mesh.vertex_stride);
	float2 uv_2 = GetMeshVertexData<float2>(mesh.vertex_buffer_id, mesh.uvs_offset, index_2, mesh.vertex_stride);
	float2 uv = Interpolate(uv_0, uv_1, uv_2, barycentrics);

	return SampleTextureLevel(material.alpha_tex_id, uv, 0).a < material.alpha_cutoff;
}

bool traceVisibility(RayDesc ray, uint ray_flags = RAY_FLAG_NONE)
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[tlas_id];
	RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> ray_query;
	ray_query.TraceRayInline(tlas, ray_flags, 0xff, ray);
	while (ray_query.Proceed())
	{
		if (!isAlphaClipped(ray_query.CandidateInstanceID(), ray_query.CandidatePrimitiveIndex(), ray_query.CandidateTriangleBarycentrics()))
			ray_query.CommitNonOpaqueTriangleHit();
	}
	return ray_query.CommittedStatus() == COMMITTED_NOTHING;
}
