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
	uint32_t version = import_settings.meshlet_mode != MESHLET_MODE_DISABLED ? VERSION | MESHLET_VERSION << 4 : VERSION;
	return version | import_settings.specular_texture_layout << 8;
}

enum MeshAttributeFlags: uint32_t
{
	MESH_ATTR_TANGENT = 1 << 1,
	MESH_ATTR_UV = 1 << 2,
};

// Meshlet verices are packed, this is format:
// [positions, attributes]
// 
// Positions of all vertices, a sequence of bits padded to 4 bytes:
// x1, y1, z1, x2, y2, z2 ... sequence. Every component of size Meshlet::position_bits.
// In shader this quantized positions just offseted (by quantized_min) and scaled
// 
// Then attributes of all vertices:
// snorm16x2 oct_normal (4 bytes)
// 8 bytes additional (MESH_ATTR_TANGENT):
//	snorm16x2 oct_tangent (4)
//	int8 tangent_sign (1)
//	pad 3 bytes
// 8 bytes additional (MESH_ATTR_UV):
//	vec2 uv (8 bytes)

inline uint16_t packPositionBits(glm::ivec3 max_offset)
{
	glm::ivec3 bits_minus_one = glm::max(glm::findMSB(max_offset), 0);
	return (uint16_t)(bits_minus_one.x | bits_minus_one.y << 4 | bits_minus_one.z << 8);
}

inline glm::uvec3 unpackPositionBits(uint16_t position_bits)
{
	return glm::uvec3(position_bits & 0xF, (position_bits >> 4) & 0xF, (position_bits >> 8) & 0xF) + 1u;
}

inline uint32_t vertexAttributesBytes(uint32_t attribute_flags)
{
	return 4 + ((attribute_flags & MESH_ATTR_TANGENT) ? 8 : 0) + ((attribute_flags & MESH_ATTR_UV) ? 8 : 0);
}

inline uint32_t meshletPositionBytes(uint32_t vertex_count, uint16_t position_bits)
{
	glm::uvec3 bits = unpackPositionBits(position_bits);
	return Math::alignedSize(vertex_count * (bits.x + bits.y + bits.z), 32u) / 8;
}

inline uint32_t meshletVertexBytes(uint32_t vertex_count, uint16_t position_bits, uint32_t attribute_flags)
{
	// Vertex layout: positions (bit sequence, up to 48 bits each vec3) padded to 4 bytes, then attributes:
	return meshletPositionBytes(vertex_count, position_bits) + vertex_count * vertexAttributesBytes(attribute_flags);
}

struct DiskMeshlet
{
	uint32_t group_id;
	uint32_t refined_group_id;
	uint32_t vertex_offset; // byte offset relative to group
	uint32_t triangle_offset; // triangle index relative to group
	int32_t quantized_min[3];
	uint16_t max_offset[3];
	uint8_t  vertex_count; // max 255, standard meshlets use <= 128
	uint8_t  triangle_count; // max 255, standard meshlets use <= 128
};
static_assert(sizeof(DiskMeshlet) == 36);

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
	float position_scale;
	uint32_t padding;
	uint64_t meshlet_vertices_file_offset;
	uint64_t meshlet_vertex_bytes;
	uint64_t meshlet_triangles_file_offset;
};
static_assert(sizeof(MeshEntry) == 112 && sizeof(MeshEntry) % 16 == 0);

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
	float emissive[3];
	float emissive_intensity;
	uint64_t albedo_tex_guid;
	uint64_t metalness_tex_guid;
	uint64_t roughness_tex_guid;
	uint64_t specular_tex_guid;
	uint64_t normal_tex_guid;
	uint64_t emissive_tex_guid;
};
static_assert(sizeof(MaterialDescriptor) == 96 && sizeof(MaterialDescriptor) % 16 == 0);

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
void encodeVertexAttributes(uint8_t *dst, const Engine::Vertex &v, uint32_t attr_flags);
void deduplicateVertices(eastl::vector<Engine::Vertex> &vertices, eastl::vector<uint32_t> &indices, eastl::vector<uint32_t> &remap);
DiskMeshlet encodeMeshlet(const Meshlet &m, float position_scale);
Meshlet decodeMeshlet(const DiskMeshlet &d, float position_scale);
MaterialDescriptor encodeMaterial(const Material *mat);
Ref<Material> decodeMaterial(const MeshFormat::MaterialDescriptor &d);
}
