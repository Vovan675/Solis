#include "pch.h"
#include "Mesh.h"
#include "Core/Variables.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "RHI/DynamicRHI.h"

namespace Engine
{
	void Mesh::setData(eastl::vector<Vertex> vertices, eastl::vector<uint32_t> indices)
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
		uint32_t additional_usage = 0;
		if (engine_ray_tracing)
			additional_usage |= ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

		// Create Vertex buffer
		BufferDescription vertexDesc;
		vertexDesc.size = sizeof(vertices[0]) * vertices.size();
		vertexDesc.useStagingBuffer = true;
		vertexDesc.usage = BufferUsage::VERTEX_BUFFER | additional_usage;

		vertexDesc.alignment = 16;
		vertexDesc.stride = sizeof(vertices[0]);

		vertexBuffer = gDynamicRHI->createBuffer(vertexDesc);
		vertexBuffer->fill(vertices.data());
		vertexBuffer->setDebugName("Vertex Buffer");

		// Create Index buffer
		BufferDescription indexDesc;
		indexDesc.size = sizeof(indices[0]) * indices.size();
		indexDesc.useStagingBuffer = true;
		indexDesc.usage = BufferUsage::INDEX_BUFFER | additional_usage;
		indexDesc.alignment = 0;

		indexBuffer = gDynamicRHI->createBuffer(indexDesc);
		indexBuffer->fill(indices.data());
		indexBuffer->setDebugName("Index Buffer");
	}
}