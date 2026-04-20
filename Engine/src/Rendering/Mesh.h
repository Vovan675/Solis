#pragma once
#include "RHI/RHIBuffer.h"
#include "Math/BoundBox.h"
#include "RHI/RHIPipeline.h"
#include "ShaderStructs.h"

namespace Engine
{
struct MeshletFileView
{
	const uint8_t *vertices_ptr = nullptr;
	const uint8_t *triangles_ptr = nullptr;
	uint32_t vertex_count = 0;
	uint32_t triangle_count = 0;
	bool isValid() const { return vertices_ptr != nullptr; }
};

struct Vertex
{
	glm::highp_vec3 pos;
	glm::highp_vec3 normal;
	glm::highp_vec3 tangent;
	float tangent_sign = 1.0f;
	glm::highp_vec2 uv;

	static VertexInputsDescription GetVertexInputsDescription()
	{
		VertexInputsDescription desc;
		desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
		desc.inputs.push_back({"NORMAL", 0, FORMAT_R32G32B32_SFLOAT});
		desc.inputs.push_back({"TANGENT",  0, FORMAT_R32G32B32A32_SFLOAT});
		desc.inputs.push_back({"TEXCOORD", 0, FORMAT_R32G32_SFLOAT});
		return desc;
	}
};

struct IndexedGeometry
{
	eastl::vector<Vertex> vertices;
	eastl::vector<uint32_t> indices;
	RHIBufferRef vertex_buffer;
	RHIBufferRef index_buffer;
};

struct MeshletGeometry
{
	struct LODGroupDataInfo
	{
		uint32_t cpu_vertex_offset;
		uint32_t cpu_vertex_count;
		uint32_t cpu_triangle_offset;
		uint32_t cpu_triangle_count;
	};

	eastl::vector<Meshlet> meshlets;
	eastl::vector<LODGroup> meshlet_lod_groups;
	eastl::vector<LodNode> lod_nodes;
	eastl::vector<LODLevel> meshlet_lod_levels;
	eastl::vector<LODGroupDataInfo> meshlet_lod_group_data_info;
	uint32_t meshlet_root_group_local_offset = 0;
};

class Mesh: public RefCounted
{
public:
	size_t id = 0;
	uint32_t attribute_flags = 0;

	glm::mat4 root_transform = glm::mat4(1.0);
	BoundBox bound_box;

	eastl::optional<IndexedGeometry> indexed;
	eastl::optional<MeshletGeometry> meshlet_data;

	bool useMeshlets() const { return meshlet_data.has_value(); }

	void initTraditional(eastl::vector<Vertex> vertices, eastl::vector<uint32_t> indices);
	void initMeshleted();

private:
	void uploadTraditionalBuffers();
	void uploadMeshletMetadata();
};
}
