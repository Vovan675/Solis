#pragma once
#include "RHI/Buffer.h"
#include "BoundBox.h"
#include "Utils/Stream.h"
#include "glm/glm.hpp"

namespace Engine
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec3 tangent;
		glm::vec2 uv;
		glm::vec3 color;

		// Description how to send data from gpu to shaders
		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription desc{};
			desc.binding = 0;
			desc.stride = sizeof(Vertex);
			desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return desc;
		}

		static std::array<VkVertexInputAttributeDescription, 5> GetAttributeDescription()
		{
			std::array<VkVertexInputAttributeDescription, 5> descs {};
			// Position
			descs[0].binding = 0;
			descs[0].location = 0;
			descs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			descs[0].offset = offsetof(Vertex, pos);

			// Normal
			descs[1].binding = 0;
			descs[1].location = 1;
			descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			descs[1].offset = offsetof(Vertex, normal);

			// Tangent
			descs[2].binding = 0;
			descs[2].location = 2;
			descs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			descs[2].offset = offsetof(Vertex, tangent);

			// UV
			descs[3].binding = 0;
			descs[3].location = 3;
			descs[3].format = VK_FORMAT_R32G32_SFLOAT;
			descs[3].offset = offsetof(Vertex, uv);

			// Color
			descs[4].binding = 0;
			descs[4].location = 4;
			descs[4].format = VK_FORMAT_R32G32B32_SFLOAT;
			descs[4].offset = offsetof(Vertex, color);
			return descs;
		}
	};

	// Just a collection of data
	class Mesh
	{
	public:
		size_t id = 0;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		std::shared_ptr<Buffer> vertexBuffer;
		std::shared_ptr<Buffer> indexBuffer;
		glm::mat4 root_transform = glm::mat4(1.0);

		BoundBox bound_box;
	public:
		Mesh() = default;
		~Mesh() = default;
		void setData(std::vector<Vertex> vertices, std::vector<uint32_t> indices);

		void serialize(Stream &stream);
		void deserialize(Stream &stream);
	private:
		void create_buffers();
	};
}