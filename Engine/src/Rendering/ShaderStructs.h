#pragma once

#define PASS_MASK_GBUFFER 1 << 1
#define PASS_MASK_DIRECTIONAL_SHADOW 1 << 2
#define PASS_MASK_POINT_SHADOW 1 << 3

struct MaterialGPU
{
	glm::vec4 albedo = glm::vec4(0, 0, 0, 1);
	uint32_t albedo_tex_id = 0;
	uint32_t metalness_tex_id = 0;
	uint32_t roughness_tex_id = 0;
	uint32_t specular_tex_id = 0;
	// metalness, roughness, specular
	glm::vec4 shading = glm::vec4(0, 0, 0.5, 1);
	uint32_t normal_tex_id = 0;
};

struct InstanceGPU
{
	glm::mat4 world_transform;
	glm::mat4 iworld_transform;

	glm::vec4 bound_sphere;
	glm::vec4 bound_center;
	glm::vec4 bound_extent;

	uint32_t mesh_id;
	uint32_t material_id = 0;
};


struct MeshGPU
{
	uint32_t vertex_buffer_id;
	uint32_t index_buffer_id; // reserved for non-meshleted rendering
	uint32_t vertex_stride;
	uint32_t positions_offset;
	uint32_t normals_offset;
	uint32_t tangents_offset;
	uint32_t uvs_offset;
	uint32_t index_offset; // reserved for non-meshleted rendering
	uint32_t indices_count;

	uint32_t meshlet_lod_groups_offset;

	uint32_t group_residency_offset; // start index in global group_residency buffer
	uint32_t root_group_offset;
	uint32_t lod_nodes_offset; // offset in global lod_nodes buffer

	uint32_t attribute_flags;
};

struct LODGroup
{
	glm::highp_vec3 center;
	float radius;
	float error;

	uint32_t depth;
	uint32_t first_meshlet; // local index into mesh's meshlets array
	uint32_t meshlet_count;
};

struct LodNode
{
	glm::highp_vec3 center;
	float radius;
	float error;

	uint32_t group_index = 0; // local LODGroup index
	uint32_t first_child = 0; // local lod_nodes offset of first child
	uint32_t child_count = 0; // 0 for leaf
	uint32_t meshlet_count = 0; // meshlet count of the associated LODGroup
};

// group range for one depth level of the LOD DAG
struct LODLevel
{
	uint32_t group_offset;
	uint32_t group_count;
};

struct MeshletTriangle
{
	uint32_t v0 : 10;
	uint32_t v1 : 10;
	uint32_t v2 : 10;
	uint32_t : 2;
};

struct Meshlet
{
	glm::highp_vec3 center;
	glm::highp_vec3 extent;

	uint32_t group_id; // current group id
	uint32_t refined_group_id; // group id of more detailed cluster group (with more triangles)

	uint32_t vertex_offset;
	uint32_t triangle_offset;
	uint8_t vertex_count; // typically 64
	uint8_t triangle_count; // typically 124
};

#define GROUP_NON_RESIDENT_ADDRESS_START uint32_t(1) << 31
static constexpr uint32_t MAX_STREAMING_REQUESTS = 1024;
static constexpr uint32_t MAX_UNLOAD_REQUESTS = 1024;
struct GroupResidency
{
	uint32_t geometry_buffer_offset; // byte offset in global geometry buffer
};

struct FrustumDataGPU
{
	glm::mat4 view_projection;
	uint32_t pass_mask;
	uint32_t is_ortho = 0;
	uint32_t pad[2];
};

struct DrawIndexedIndirect
{
	uint32_t index_count_per_instance;
	uint32_t instance_count;
	uint32_t start_index_location;
	uint32_t base_vertex_location;
	uint32_t start_instance_location;
};

struct DrawIndirect
{
	uint32_t vertex_count_per_instance;
	uint32_t instance_count;
	uint32_t first_vertex;
	uint32_t first_instance;
};

struct DispatchIndirect
{
	uint32_t group_x;
	uint32_t group_y;
	uint32_t group_z;
};