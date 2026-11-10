#pragma once
#include "RHI/RHIBuffer.h"
#include "Math/BoundBox.h"
#include "RHI/RHIPipeline.h"
#include "ShaderStructs.h"

namespace Engine
{
struct MeshletFileView
{
	const uint8_t *pages_data = nullptr; // MeshletFileView::pages_data like data from mapped file 
	bool isValid() const { return pages_data != nullptr; }
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
	// Sections: [meshlets][vertices][1 byte indices], the indices are meshopt encoded in the file
	struct Page
	{
		uint64_t file_offset; // in bytes, from MeshletFileView::pages_data
		uint32_t meshlets_bytes;
		uint32_t vertices_bytes;
		uint32_t encoded_triangles_bytes; // triangles of a meshlet: uint8 triangle count, uint16 size, meshopt_encodeMeshlet bytes
		uint32_t index_count;

		uint32_t getVerticesOffset() const { return meshlets_bytes; }
		uint32_t getIndicesOffset() const { return meshlets_bytes + vertices_bytes; }
		uint32_t getFileBytes() const { return getIndicesOffset() + encoded_triangles_bytes; }
		uint32_t getGpuBytes() const { return getIndicesOffset() + index_count; }
	};

	eastl::vector<LODGroup> meshlet_lod_groups;
	eastl::vector<LodNode> lod_nodes;
	eastl::vector<LODLevel> meshlet_lod_levels;
	eastl::vector<Page> pages; // from coarsest to detailed. First page is always resident
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
