#pragma once
#include "RHI/RHIBuffer.h"
#include "Math/BoundBox.h"
#include "Utils/Stream.h"
#include "glm/glm.hpp"
#include "RHI/RHIPipeline.h"
#include "ShaderStructs.h"

namespace Engine
{
	struct Vertex
	{
		glm::highp_vec3 pos;
		glm::highp_vec3 normal;
		glm::highp_vec3 tangent;
		glm::highp_vec2 uv;
		glm::highp_vec3 color;

		static VertexInputsDescription GetVertexInputsDescription()
		{
			VertexInputsDescription desc;
			desc.inputs.push_back({"POSITION", 0, FORMAT_R32G32B32_SFLOAT});
			desc.inputs.push_back({"NORMAL", 0, FORMAT_R32G32B32_SFLOAT});
			desc.inputs.push_back({"TANGENT", 0, FORMAT_R32G32B32_SFLOAT});
			desc.inputs.push_back({"TEXCOORD", 0, FORMAT_R32G32_SFLOAT});
			desc.inputs.push_back({"COLOR", 0, FORMAT_R32G32B32_SFLOAT});
			return desc;
		}
	};

	// Just a collection of data
	class Mesh : public RefCounted
	{
	public:
		size_t id = 0;
		eastl::vector<Vertex> vertices;
		eastl::vector<uint32_t> indices;
		eastl::vector<Meshlet> meshlets;

		eastl::vector<uint32_t> meshlet_vertices; // indices to global vertex buffer
		eastl::vector<uint32_t> meshlet_triangles; // TODO: just for now, while not working with mesh shaders, for index buffer compatibility
		//eastl::vector<MeshletTriangle> meshlet_triangles; // index to meshlet_vertices vertices

		uint64_t global_vertex_buffer_offset = 0;
		uint64_t global_index_buffer_offset = 0;
		uint64_t global_meshlet_vertex_offset = 0;
		uint64_t global_meshlet_triangle_offset = 0;

		RHIBufferRef vertexBuffer;
		RHIBufferRef indexBuffer;
		glm::mat4 root_transform = glm::mat4(1.0);

		BoundBox bound_box;
	public:
		Mesh() = default;
		~Mesh() = default;
		void setData(eastl::vector<Vertex> vertices, eastl::vector<uint32_t> indices);

		void serialize(Stream &stream);
		void deserialize(Stream &stream);
	private:
		void create_buffers();
	};
}