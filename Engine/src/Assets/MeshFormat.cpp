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

	float uv[2] = {v.uv.x, v.uv.y};
	memcpy(dst + 16, uv, 8);

	if (attr_flags & MESH_ATTR_TANGENT)
	{
		auto oct_t = Engine::Math::octEncode(v.tangent);
		int16_t oct_tangent[2] = {
			(int16_t)meshopt_quantizeSnorm(oct_t.x, 16),
			(int16_t)meshopt_quantizeSnorm(oct_t.y, 16),
		};
		memcpy(dst + 24, oct_tangent, 4);
		int8_t sign = v.tangent_sign >= 0.0f ? 1 : -1;
		dst[28] = sign;
		dst[29] = 0;
		dst[30] = 0;
		dst[31] = 0;
		return 32;
	}
	return 24;
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

	float uv[2];
	memcpy(uv, src + 16, 8);
	v.uv = glm::vec2(uv[0], uv[1]);

	if (attr_flags & MESH_ATTR_TANGENT)
	{
		int16_t oct_tangent[2];
		memcpy(oct_tangent, src + 24, 4);
		v.tangent = Engine::Math::octDecode(oct_tangent[0] / 32767.0f, oct_tangent[1] / 32767.0f);
		v.tangent_sign = (float)(int8_t)src[28];
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
	d.albedo_tex_guid = mat->albedo_tex.asset_handle;
	d.metalness_tex_guid = mat->metalness_tex.asset_handle;
	d.roughness_tex_guid = mat->roughness_tex.asset_handle;
	d.specular_tex_guid = mat->specular_tex.asset_handle;
	d.normal_tex_guid = mat->normal_tex.asset_handle;
	return d;
}

Ref<Material> decodeMaterial(const MeshFormat::MaterialDescriptor &d)
{
	Ref<Material> mat = new Material();
	mat->albedo = glm::vec4(d.albedo[0], d.albedo[1], d.albedo[2], d.albedo[3]);
	mat->metalness = d.metalness; mat->roughness = d.roughness; mat->specular = d.specular;
	mat->albedo_tex.asset_handle = d.albedo_tex_guid;
	mat->metalness_tex.asset_handle = d.metalness_tex_guid;
	mat->roughness_tex.asset_handle = d.roughness_tex_guid;
	mat->specular_tex.asset_handle = d.specular_tex_guid;
	mat->normal_tex.asset_handle = d.normal_tex_guid;
	return mat;
}

}
