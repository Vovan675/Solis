#include "pch.h"
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace Engine
{
	void Mesh::setData(std::vector<Vertex> vertices, std::vector<uint32_t> indices)
	{
		this->vertices = vertices;
		this->indices = indices;
		
		bound_box = BoundBox();
		for (const auto &vertex : vertices)
			bound_box.extend(vertex.pos);

		create_buffers();
	}

	void Mesh::create_buffers()
	{
		// Create Vertex buffer
		BufferDescription vertexDesc;
		vertexDesc.size = sizeof(vertices[0]) * vertices.size();
		vertexDesc.useStagingBuffer = true;
		vertexDesc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		vertexBuffer = std::make_shared<Buffer>(vertexDesc);
		vertexBuffer->fill(vertices.data());

		// Create Index buffer
		BufferDescription indexDesc;
		indexDesc.size = sizeof(indices[0]) * indices.size();
		indexDesc.useStagingBuffer = true;
		indexDesc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

		indexBuffer = std::make_shared<Buffer>(indexDesc);
		indexBuffer->fill(indices.data());
	}
}