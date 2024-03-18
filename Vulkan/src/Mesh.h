#pragma once
#include "RHI/Buffer.h"
#include "glm/glm.hpp"

namespace Engine
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
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

		static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescription()
		{
			std::array<VkVertexInputAttributeDescription, 4> descs {};
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

			// UV
			descs[2].binding = 0;
			descs[2].location = 2;
			descs[2].format = VK_FORMAT_R32G32_SFLOAT;
			descs[2].offset = offsetof(Vertex, uv);

			// Color
			descs[3].binding = 0;
			descs[3].location = 3;
			descs[3].format = VK_FORMAT_R32G32B32_SFLOAT;
			descs[3].offset = offsetof(Vertex, color);
			return descs;
		}
	};

	// Just a collection of data
	class Mesh
	{
	public:
		std::string filePath;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		std::shared_ptr<Buffer> vertexBuffer;
		std::shared_ptr<Buffer> indexBuffer;
		glm::mat4 root_transform;
	public:
		Mesh() = default;
		Mesh(const std::string& path);
		~Mesh();
		void loadFromPath();
		void setData(std::vector<Vertex> vertices, std::vector<uint32_t> indices);
	private:
		void create_buffers();
	};
}