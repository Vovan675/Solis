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
	uint32_t index_buffer_id;
	uint32_t vertex_stride;
	uint32_t positions_offset;
	uint32_t normals_offset;
	uint32_t tangents_offset;
	uint32_t uvs_offset;
	uint32_t colors_offset;
	uint32_t index_offset;
	uint32_t indices_count;

	uint32_t meshlet_vertex_offset;
	uint32_t meshlet_triangle_offset;

	uint32_t meshlet_offset;
	uint32_t meshlet_count;
};

struct MeshletTriangle
{
	uint32_t v0 : 10;
	uint32_t v1 : 10;
	uint32_t v2 : 10;
	uint32_t : 2;
};

struct alignas(16) Meshlet
{
	glm::vec3 center;
	glm::vec3 extent;

	uint32_t vertex_offset;
	uint32_t vertex_count; // typically 64
	uint32_t triangle_offset;
	uint32_t triangle_count; // typically 124
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