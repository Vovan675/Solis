#include "pch.h"
#include "MeshFormat.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Math/EngineMath.h"
#include "meshoptimizer.h"

namespace MeshFormat
{

void encodeVertexAttributes(uint8_t *dst, const Engine::Vertex &v, uint32_t attr_flags)
{
	auto oct_n = Engine::Math::octEncode(v.normal);
	int16_t oct_normal[2] = {
		(int16_t)meshopt_quantizeSnorm(oct_n.x, 16),
		(int16_t)meshopt_quantizeSnorm(oct_n.y, 16),
	};
	memcpy(dst, oct_normal, 4);
	uint32_t offset = 4;

	if (attr_flags & MESH_ATTR_TANGENT)
	{
		auto oct_t = Engine::Math::octEncode(v.tangent);
		int16_t oct_tangent[2] = {
			(int16_t)meshopt_quantizeSnorm(oct_t.x, 16),
			(int16_t)meshopt_quantizeSnorm(oct_t.y, 16),
		};
		memcpy(dst + offset, oct_tangent, 4);
		int8_t sign = v.tangent_sign >= 0.0f ? 1 : -1;
		dst[offset + 4] = sign;
		dst[offset + 5] = 0;
		dst[offset + 6] = 0;
		dst[offset + 7] = 0;
		offset += 8;
	}

	if (attr_flags & MESH_ATTR_UV)
	{
		float uv[2] = {v.uv.x, v.uv.y};
		memcpy(dst + offset, uv, 8);
	}
}

void deduplicateVertices(eastl::vector<Engine::Vertex> &vertices, eastl::vector<uint32_t> &indices, eastl::vector<uint32_t> &remap)
{
	remap.resize(vertices.size());
	size_t unique_vertex_count = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(Engine::Vertex));
	if (unique_vertex_count < vertices.size())
	{
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());
		meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vertices.size(), sizeof(Engine::Vertex), remap.data());
		vertices.resize(unique_vertex_count);
	}
}

DiskMeshlet encodeMeshlet(const Meshlet &m, float position_scale)
{
	ENGINE_ASSERT(m.vertex_count <= 255 && m.triangle_count <= 255);
	DiskMeshlet d{};
	d.group_id = m.group_id;
	d.refined_group_id = m.refined_group_id;
	d.vertex_offset = m.vertex_offset;
	d.triangle_offset = m.triangle_offset;
	d.quantized_min[0] = (int32_t)(m.bound_min.x / position_scale);
	d.quantized_min[1] = (int32_t)(m.bound_min.y / position_scale);
	d.quantized_min[2] = (int32_t)(m.bound_min.z / position_scale);
	d.max_offset[0] = (uint16_t)(m.bound_extent.x * 2.0f / position_scale);
	d.max_offset[1] = (uint16_t)(m.bound_extent.y * 2.0f / position_scale);
	d.max_offset[2] = (uint16_t)(m.bound_extent.z * 2.0f / position_scale);
	d.vertex_count = m.vertex_count;
	d.triangle_count = m.triangle_count;
	return d;
}

Meshlet decodeMeshlet(const DiskMeshlet &d, float position_scale)
{
	Meshlet m{};
	m.group_id = d.group_id;
	m.refined_group_id = d.refined_group_id;
	m.vertex_offset = d.vertex_offset;
	m.triangle_offset = d.triangle_offset;
	m.bound_min.x = d.quantized_min[0] * position_scale;
	m.bound_min.y = d.quantized_min[1] * position_scale;
	m.bound_min.z = d.quantized_min[2] * position_scale;
	m.bound_extent.x = d.max_offset[0] * position_scale * 0.5f;
	m.bound_extent.y = d.max_offset[1] * position_scale * 0.5f;
	m.bound_extent.z = d.max_offset[2] * position_scale * 0.5f;
	m.vertex_count = d.vertex_count;
	m.triangle_count = d.triangle_count;
	m.position_bits = packPositionBits(glm::ivec3(d.max_offset[0], d.max_offset[1], d.max_offset[2]));
	return m;
}

MeshFormat::MaterialDescriptor encodeMaterial(const Material *mat)
{
	MeshFormat::MaterialDescriptor d{};
	d.albedo[0] = mat->albedo.r;
	d.albedo[1] = mat->albedo.g;
	d.albedo[2] = mat->albedo.b;
	d.albedo[3] = mat->albedo.a;
	d.metalness = mat->metalness;
	d.roughness = mat->roughness;
	d.specular = mat->specular;
	d.emissive[0] = mat->emissive.r;
	d.emissive[1] = mat->emissive.g;
	d.emissive[2] = mat->emissive.b;
	d.emissive_intensity = mat->emissive_intensity;
	d.albedo_tex_guid = mat->albedo_tex.asset.guid;
	d.metalness_tex_guid = mat->metalness_tex.asset.guid;
	d.roughness_tex_guid = mat->roughness_tex.asset.guid;
	d.specular_tex_guid = mat->specular_tex.asset.guid;
	d.normal_tex_guid = mat->normal_tex.asset.guid;
	d.emissive_tex_guid = mat->emissive_tex.asset.guid;
	return d;
}

Ref<Material> decodeMaterial(const MeshFormat::MaterialDescriptor &d)
{
	Ref<Material> mat = new Material();
	mat->albedo = glm::vec4(d.albedo[0], d.albedo[1], d.albedo[2], d.albedo[3]);
	mat->metalness = d.metalness;
	mat->roughness = d.roughness;
	mat->specular = d.specular;
	mat->emissive = glm::vec4(d.emissive[0], d.emissive[1], d.emissive[2], 1.0f);
	mat->emissive_intensity = d.emissive_intensity;
	mat->albedo_tex.asset.guid = d.albedo_tex_guid;
	mat->metalness_tex.asset.guid = d.metalness_tex_guid;
	mat->roughness_tex.asset.guid = d.roughness_tex_guid;
	mat->specular_tex.asset.guid = d.specular_tex_guid;
	mat->normal_tex.asset.guid = d.normal_tex_guid;
	mat->emissive_tex.asset.guid = d.emissive_tex_guid;
	return mat;
}

}
