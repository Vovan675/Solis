#include "pch.h"
#include "MeshFormat.h"
#include "Rendering/Mesh.h"
#include "Rendering/Material.h"
#include "Math/EngineMath.h"
#include "meshoptimizer.h"

namespace MeshFormat
{

uint32_t encodeVertex(uint8_t *dst, const Engine::Vertex &v, uint32_t attr_flags)
{
	float pos[3] = {v.pos.x, v.pos.y, v.pos.z};
	memcpy(dst + 0, pos, 12);

	auto oct_n = Engine::Math::octEncode(v.normal);
	int16_t oct_normal[2] = {
		(int16_t)meshopt_quantizeSnorm(oct_n.x, 16),
		(int16_t)meshopt_quantizeSnorm(oct_n.y, 16),
	};
	memcpy(dst + 12, oct_normal, 4);
	uint32_t offset = 16;

	if (attr_flags & MESH_ATTR_UV)
	{
		float uv[2] = {v.uv.x, v.uv.y};
		memcpy(dst + offset, uv, 8);
		offset += 8;
	}

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
	return offset;
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

Engine::Vertex decodeVertex(const uint8_t *src, uint32_t attr_flags)
{
	Engine::Vertex v{};

	float pos[3];
	memcpy(pos, src + 0, 12);
	v.pos = glm::vec3(pos[0], pos[1], pos[2]);

	int16_t oct_normal[2];
	memcpy(oct_normal, src + 12, 4);
	v.normal = Engine::Math::octDecode(oct_normal[0] / 32767.0f, oct_normal[1] / 32767.0f);
	uint32_t offset = 16;

	if (attr_flags & MESH_ATTR_UV)
	{
		float uv[2];
		memcpy(uv, src + offset, 8);
		v.uv = glm::vec2(uv[0], uv[1]);
		offset += 8;
	}

	if (attr_flags & MESH_ATTR_TANGENT)
	{
		int16_t oct_tangent[2];
		memcpy(oct_tangent, src + offset, 4);
		v.tangent = Engine::Math::octDecode(oct_tangent[0] / 32767.0f, oct_tangent[1] / 32767.0f);
		v.tangent_sign = (float)(int8_t)src[offset + 4];
	}
	return v;
}

DiskMeshlet encodeMeshlet(const Meshlet &m)
{
	ENGINE_ASSERT(m.vertex_count <= 255 && m.triangle_count <= 255);
	DiskMeshlet d{};
	d.group_id = m.group_id;
	d.refined_group_id = m.refined_group_id;
	d.vertex_offset = m.vertex_offset;
	d.triangle_offset = m.triangle_offset;
	d.center[0] = Math::pack32to16(m.center.x);
	d.center[1] = Math::pack32to16(m.center.y);
	d.center[2] = Math::pack32to16(m.center.z);
	d.extent[0] = Math::pack32to16(m.extent.x);
	d.extent[1] = Math::pack32to16(m.extent.y);
	d.extent[2] = Math::pack32to16(m.extent.z);
	d.vertex_count = m.vertex_count;
	d.triangle_count = m.triangle_count;
	return d;
}

Meshlet decodeMeshlet(const DiskMeshlet &d)
{
	Meshlet m{};
	m.group_id = d.group_id;
	m.refined_group_id = d.refined_group_id;
	m.vertex_offset = d.vertex_offset;
	m.triangle_offset = d.triangle_offset;
	m.center.x = Math::unpack16to32(d.center[0]);
	m.center.y = Math::unpack16to32(d.center[1]);
	m.center.z = Math::unpack16to32(d.center[2]);
	m.extent.x = Math::unpack16to32(d.extent[0]);
	m.extent.y = Math::unpack16to32(d.extent[1]);
	m.extent.z = Math::unpack16to32(d.extent[2]);
	m.vertex_count = d.vertex_count;
	m.triangle_count = d.triangle_count;
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
