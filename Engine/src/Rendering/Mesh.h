#pragma once
#include "RHI/RHIBuffer.h"
#include "Math/BoundBox.h"
#include "RHI/RHIPipeline.h"
#include "ShaderStructs.h"

namespace Engine
{
struct MeshletFileView
{
	const uint8_t *groups_data = nullptr; // MeshletFileView::groups_data like data from mapped file 
	bool isValid() const { return groups_data != nullptr; }
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
		desc.inputs.push_back({"POSITION", 0, Format::R32G32B32_SFLOAT});
		desc.inputs.push_back({"NORMAL", 0, Format::R32G32B32_SFLOAT});
		desc.inputs.push_back({"TANGENT",  0, Format::R32G32B32A32_SFLOAT});
		desc.inputs.push_back({"TEXCOORD", 0, Format::R32G32_SFLOAT});
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
		uint64_t file_offset; // in bytes, from MeshletFileView::groups_data
		uint32_t gpu_bytes; // [Meshlets][vertices][1 byte indices] size
	};

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
	float position_scale = 0.0f;

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
