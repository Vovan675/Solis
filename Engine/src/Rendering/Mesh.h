#pragma once
#include "RHI/RHIBuffer.h"
#include "Math/BoundBox.h"
#include "Utils/Stream.h"
#include "glm/glm.hpp"
#include "RHI/RHIPipeline.h"

namespace Engine
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec2 uv;
		glm::vec3 color;

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