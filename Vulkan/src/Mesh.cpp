#include "pch.h"
#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "Variables.h"

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

	void Mesh::serialize(Stream &stream)
	{
		stream.write(id);
		stream.write(vertices, true);
		stream.write(indices, true);
	}

	void Mesh::deserialize(Stream &stream)
	{
		stream.read(id);
		stream.read(vertices, true);
		stream.read(indices, true);

		bound_box = BoundBox();
		for (const auto &vertex : vertices)
			bound_box.extend(vertex.pos);

		create_buffers();
	}

	void Mesh::create_buffers()
	{
		VkBufferUsageFlags additional_flags = 0;
		if (engine_ray_tracing)
			additional_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		// Create Vertex buffer
		BufferDescription vertexDesc;
		vertexDesc.size = sizeof(vertices[0]) * vertices.size();
		vertexDesc.useStagingBuffer = true;
		vertexDesc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | additional_flags;

		vertexDesc.alignment = 16;

		vertexBuffer = Buffer::create(vertexDesc);
		vertexBuffer->fill(vertices.data());
		vertexBuffer->setDebugName("Vertex Buffer");

		// Create Index buffer
		BufferDescription indexDesc;
		indexDesc.size = sizeof(indices[0]) * indices.size();
		indexDesc.useStagingBuffer = true;
		indexDesc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | additional_flags;
		vertexDesc.alignment = 16;

		indexBuffer = Buffer::create(indexDesc);
		indexBuffer->fill(indices.data());
		indexBuffer->setDebugName("Index Buffer");
	}
}