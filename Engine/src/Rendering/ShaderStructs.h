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
	glm::vec4 emissive = glm::vec4(0, 0, 0, 1);
	uint32_t normal_tex_id = 0;
	uint32_t emissive_tex_id = 0;
};

#define INVALID_LIGHT_INDEX 0xFFFFFFFFu

struct LightGPU
{
	glm::vec4 position;
	glm::vec4 direction;
	glm::vec4 radiance;
	glm::mat4 cascade_view_projection[4];
	glm::vec4 cascade_splits;
	uint32_t type;
	float attenuation_radius;
	uint32_t shadow_map_tex_id;
};

// [Directional Lights ... Local Lights ...]
struct LightTableLayout
{
	uint32_t directional_count = 0;
	uint32_t local_count = 0;
};

#define INSTANCE_FLAG_INVALID 0x1

struct InstanceGPU
{
	glm::mat4 world_transform;
	glm::mat4 iworld_transform;
	glm::mat4 old_world_transform;

	glm::vec4 bound_sphere;
	glm::vec4 bound_center;
	glm::vec4 bound_extent;

	uint32_t mesh_id;
	uint32_t material_id = 0;
	uint32_t flags = 0;
};

#define MESH_FLAG_MESHLET 0x1

struct MeshGPU
{
	// Shared
	uint32_t vertex_buffer_id;
	uint32_t vertex_stride;
	uint32_t positions_offset;
	uint32_t normals_offset;
	uint32_t tangents_offset;
	uint32_t uvs_offset;
	uint32_t attribute_flags;
	uint32_t flags;

	// Traditional (non-meshleted)
	uint32_t index_buffer_id;
	uint32_t index_offset;
	uint32_t indices_count;

	// Meshleted
	uint32_t meshlet_lod_groups_offset;
	uint32_t page_table_offset; // first page of the mesh in the global page table
	uint32_t root_group_offset;
	uint32_t lod_nodes_offset; // offset in global lod_nodes buffer
	float position_scale; // quantized positions scaler
};

struct LODGroup
{
	glm::highp_vec3 center;
	float radius;
	float error;

	uint32_t meshlets_location;
	uint32_t page_count; // the group may continue in the next pages
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

static constexpr uint32_t MOST_DETAILED_CLUSTER_GROUP_ID = 0xFFFFFFFFu;

struct Meshlet
{
	glm::highp_vec3 bound_min;
	glm::highp_vec3 bound_extent;

	uint32_t group_id; // current group id
	uint32_t refined_group_id; // group id of more detailed cluster group (with more triangles)

	// Location have page_id in it and offset inside page, so it needs to be unpacked
	uint32_t vertex_location;
	uint32_t triangle_location;

	uint8_t vertex_count; // typically 64
	uint8_t triangle_count; // typically 124
	uint16_t position_bits; // x, y, z bits count minus one, 4 bits each

	bool isMostDetailed() const
	{
		return refined_group_id == MOST_DETAILED_CLUSTER_GROUP_ID;
	}
};
static_assert(sizeof(Meshlet) == 44);

static constexpr uint32_t MESHLET_PAGE_SIZE_BITS = 17;
static constexpr uint32_t MESHLET_PAGE_SIZE = 1 << MESHLET_PAGE_SIZE_BITS; // 128 KB
static constexpr uint32_t MESHLET_PAGE_NOT_RESIDENT = 0xFFFFFFFFu;
static constexpr uint32_t MAX_STREAMING_REQUESTS = 1024;

static constexpr uint32_t ROOT_PAGE_SIZE = 1 << (MESHLET_PAGE_SIZE_BITS - 1); // 64 KB


// Location of meshlet data: page << MESHLET_PAGE_SIZE_BITS | byte offset in the page
inline uint32_t packPageLocation(uint32_t page, uint32_t offset)
{
	return page << MESHLET_PAGE_SIZE_BITS | offset;
}

inline uint32_t unpackLocationPage(uint32_t location)
{
	return location >> MESHLET_PAGE_SIZE_BITS;
}

inline uint32_t unpackLocationOffset(uint32_t location)
{
	return location & (MESHLET_PAGE_SIZE - 1);
}

struct CullViewGPU
{
	glm::mat4 view_projection;
	uint32_t pass_mask;
	uint32_t is_ortho = 0;
	uint32_t near_clip = 1;
	uint32_t pad;
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

struct DDGICascadeGPU
{
	glm::vec4 min;
	glm::vec4 spacing;
};

struct DDGIVolumeGPU
{
	glm::vec3 origin;
	glm::ivec4 size;
	glm::vec3 spacing;
	float use_relocation;
	float use_classification;
	uint32_t cascades_count;
	DDGICascadeGPU cascades[5];
	uint32_t rays_per_probe;
	uint32_t getProbesCount() const { return size.x * size.y * size.z * cascades_count; }
};

struct FrameConstants
{
	float time = 0.0f;
	uint32_t frame = 0;
	float sky_intensity = 1.0f;
	float camera_exposure = 1.0f;
	uint32_t lights_buffer_id = 0;
	uint32_t old_lights_buffer_id = 0;
	uint32_t light_index_mapping_buffer_id = 0;
	uint32_t lights_count = 0;
	uint32_t global_meshlets_geometry_buffer_id = 0;
	uint32_t global_meshlets_lod_groups_buffer_id = 0;
	uint32_t global_lod_nodes_buffer_id = 0;
	uint32_t materials_buffer_id = 0;
	uint32_t instances_buffer_id = 0;
	uint32_t meshes_buffer_id = 0;
	uint32_t tlas_id = 0;
	uint32_t lines_gpu_buffer_id = 0;
	DDGIVolumeGPU ddgi_volume{};
};

struct PassConstants
{
	glm::mat4 view;
	glm::mat4 iview;
	glm::mat4 projection;
	glm::mat4 iprojection;
	glm::mat4 projection_unjittered;
	glm::mat4 view_projection;
	glm::mat4 view_projection_unjittered;
	glm::mat4 old_view;
	glm::mat4 old_iview;
	glm::mat4 old_iprojection;
	glm::mat4 old_projection_unjittered;
	glm::mat4 old_view_projection_unjittered;
	glm::vec4 camera_position;
	glm::vec4 old_camera_position;
	glm::vec4 render_resolution; // (w, h, 1/w, 1/h)
	glm::vec4 output_resolution; // (w, h, 1/w, 1/h)
	glm::vec2 jitter;
	glm::vec2 old_jitter;
	float z_near;
	float z_far;
	float upscale_factor;
};
