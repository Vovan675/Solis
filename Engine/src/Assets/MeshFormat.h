#pragma once
#include "Utils/Math.h"
#include "Core/Ref.h"
#include "Assets/ModelImportSettings.h"

// Binary .mesh format
// Layout: Header, OffsetTable, MeshBlock[N], MaterialDescriptor[N], NodeDescriptor[N], PrimitiveRef[N], MeshEntry[N].
// MeshBlock: vertices, indices, meshlet_vertices, meshlet_triangles, meshlets, lod_groups, lod_nodes, lod_levels.

namespace MeshFormat
{

static constexpr uint64_t MAGIC = 0x4853454D4E474E45ULL; // "ENGNMESH"
static constexpr uint32_t VERSION = 1;
static constexpr uint32_t MESHLET_VERSION = 1;
static constexpr uint64_t ALIGNMENT = 16;

inline uint32_t calcRuntimeVersion(const ModelImportSettings &import_settings)
{
	return import_settings.meshlet_mode != MESHLET_MODE_DISABLED ? VERSION | MESHLET_VERSION << 4 : VERSION;
}

enum MeshAttributeFlags: uint32_t
{
	MESH_ATTR_TANGENT = 1 << 1,
};

// Vertex on disk is packed, this is format:
// 24 bytes:
//	vec3 pos (12 bytes)
//	snorm16x2 oct_normal (4 bytes)
//	vec2 uv (8 bytes)
// 8 bytes additional (MESH_ATTR_TANGENT): 
//	snorm16x2 oct_tangent (4)
//	int8 tangent_sign (1)
//	pad 3 bytes
inline uint32_t diskVertexStride(uint32_t attribute_flags)
{
	return (attribute_flags & MESH_ATTR_TANGENT) ? 32u : 24u;
}

struct DiskMeshlet
{
	uint32_t group_id;
	uint32_t refined_group_id;
	uint32_t vertex_offset; // vertex index relative to group
	uint32_t triangle_offset; // triangle index relative to group
	uint16_t center[3];
	uint16_t extent[3];
	uint8_t  vertex_count; // max 255, standard meshlets use <= 128
	uint8_t  triangle_count; // max 255, standard meshlets use <= 128
};
static_assert(sizeof(DiskMeshlet) == 32);

struct alignas(16) Header
{
	uint64_t magic = MAGIC;
	uint32_t version = VERSION;
	uint32_t node_count = 0;
	uint32_t mesh_count = 0;
	uint32_t material_count = 0;

	bool isValid() const { return magic == MAGIC && version == VERSION; }
};
static_assert(sizeof(Header) == 32 && sizeof(Header) % 16 == 0);

struct MeshEntry
{
	uint64_t file_offset;
	uint64_t mesh_id;
	float bbox_min[3];
	uint32_t vertex_count;
	float bbox_max[3];
	uint32_t index_count;
	uint32_t meshlet_vertex_count;
	uint32_t meshlet_triangle_count;
	uint32_t meshlet_count;
	uint32_t lod_group_count;
	uint32_t lod_node_count;
	uint32_t lod_level_count;
	uint32_t meshlet_root_group_local_offset;
	uint32_t attribute_flags;
	uint64_t meshlet_vertices_file_offset;
	uint64_t meshlet_triangles_file_offset;
};
static_assert(sizeof(MeshEntry) == 96 && sizeof(MeshEntry) % 16 == 0);

struct PrimitiveRef
{
	uint32_t mesh_idx;
	uint32_t material_idx;
};
static_assert(sizeof(PrimitiveRef) == 8);

struct alignas(16) MaterialDescriptor
{
	float albedo[4];
	float metalness;
	float roughness;
	float specular;
	uint64_t albedo_tex_guid;
	uint64_t metalness_tex_guid;
	uint64_t roughness_tex_guid;
	uint64_t specular_tex_guid;
	uint64_t normal_tex_guid;
};
static_assert(sizeof(MaterialDescriptor) == 80 && sizeof(MaterialDescriptor) % 16 == 0);

struct alignas(16) NodeDescriptor
{
	char name[64];
	float local_transform[16];
	int32_t parent_index;
	uint32_t primitive_count;
	uint32_t first_primitive_ref;
};
static_assert(sizeof(NodeDescriptor) == 144 && sizeof(NodeDescriptor) % 16 == 0);

struct OffsetTable
{
	uint64_t mesh_entries_offset;
	uint64_t materials_offset;
	uint64_t primitive_refs_offset;
	uint64_t nodes_offset;
};
static_assert(sizeof(OffsetTable) == 32 && sizeof(OffsetTable) % 16 == 0);

}

namespace Engine { struct Vertex; }
struct Meshlet;
struct Material;

namespace MeshFormat
{
uint32_t encodeVertex(uint8_t *dst, const Engine::Vertex &v, uint32_t attr_flags);
Engine::Vertex decodeVertex(const uint8_t *src, uint32_t attr_flags);
DiskMeshlet encodeMeshlet(const Meshlet &m);
Meshlet decodeMeshlet(const DiskMeshlet &d);
MaterialDescriptor encodeMaterial(const Material *mat);
Ref<Material> decodeMaterial(const MeshFormat::MaterialDescriptor &d);
}
