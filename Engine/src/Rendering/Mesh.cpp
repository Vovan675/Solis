#include "pch.h"
#include "Mesh.h"
#include "Core/Variables.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "RHI/DynamicRHI.h"
#include "GlobalBufferCache.h"

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
		BufferUsage additional_usage = BufferUsage::SHADER_READ_BUFFER;
		if (engine_ray_tracing)
			additional_usage |= BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

		// Create Vertex buffer
		BufferDescription vertexDesc;
		vertexDesc.size = sizeof(vertices[0]) * vertices.size();
		vertexDesc.use_staging_buffer = true;
		vertexDesc.usage = BufferUsage::VERTEX_BUFFER | additional_usage;
		vertexDesc.storage_stride = sizeof(uint32_t);

		vertexDesc.alignment = 16;

		vertexBuffer = gDynamicRHI->createBuffer(vertexDesc);
		vertexBuffer->fill(vertices.data());
		vertexBuffer->setDebugName("Vertex Buffer");

		// Create Index buffer
		BufferDescription indexDesc;
		indexDesc.size = sizeof(indices[0]) * indices.size();
		indexDesc.use_staging_buffer = true;
		indexDesc.usage = BufferUsage::INDEX_BUFFER | additional_usage;
		indexDesc.alignment = 0;
		indexDesc.storage_stride = sizeof(uint32_t);

		indexBuffer = gDynamicRHI->createBuffer(indexDesc);
		indexBuffer->fill(indices.data());
		indexBuffer->setDebugName("Index Buffer");

		global_vertex_buffer_offset = GlobalBufferCache::addVertexBufferData(vertices.data(), vertices.size());
		global_index_buffer_offset = GlobalBufferCache::addIndexBufferData(indices.data(), indices.size());

		if (!meshlets.empty())
		{
			global_meshlet_vertex_offset = GlobalBufferCache::addMeshletVertexData(meshlet_vertices.data(), meshlet_vertices.size());
			global_meshlet_triangle_offset = GlobalBufferCache::addMeshletTriangleData(meshlet_triangles.data(), meshlet_triangles.size());
		}
	}
}