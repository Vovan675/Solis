#include "pch.h"
#include "Mesh.h"
#include "Core/Variables.h"
#include "RHI/DynamicRHI.h"
#include "GlobalBufferCache.h"

namespace Engine
{
	void Mesh::initTraditional(eastl::vector<Vertex> vertices, eastl::vector<uint32_t> indices)
	{
		indexed.emplace();
		indexed->vertices = std::move(vertices);
		indexed->indices = std::move(indices);
		uploadTraditionalBuffers();
	}

	void Mesh::initMeshleted()
	{
		uploadMeshletMetadata();
	}

	void Mesh::uploadTraditionalBuffers()
	{
		BufferUsage extra = BufferUsage::SHADER_READ_BUFFER;
		if (engine_ray_tracing)
			extra |= BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

		BufferDescription vd;
		vd.size = sizeof(indexed->vertices[0]) * indexed->vertices.size();
		vd.use_staging_buffer = true;
		vd.usage = BufferUsage::VERTEX_BUFFER | extra;
		vd.storage_stride = sizeof(uint32_t);
		vd.alignment = 16;
		indexed->vertex_buffer = gDynamicRHI->createBuffer(vd);
		indexed->vertex_buffer->fill(indexed->vertices.data());
		indexed->vertex_buffer->setDebugName("Vertex Buffer");

		BufferDescription id;
		id.size = sizeof(indexed->indices[0]) * indexed->indices.size();
		id.use_staging_buffer = true;
		id.usage = BufferUsage::INDEX_BUFFER | extra;
		id.alignment = 0;
		id.storage_stride = sizeof(uint32_t);
		indexed->index_buffer = gDynamicRHI->createBuffer(id);
		indexed->index_buffer->fill(indexed->indices.data());
		indexed->index_buffer->setDebugName("Index Buffer");
	}

	void Mesh::uploadMeshletMetadata()
	{
		RHICommandList *cmd_list = gDynamicRHI->getCmdList();
		uint64_t lod_groups_offset = GlobalBufferCache::addMeshletLodGroupData(
			meshlet_data->meshlet_lod_groups.data(), meshlet_data->meshlet_lod_groups.size(), cmd_list);

		uint64_t lod_nodes_offset = 0;
		if (!meshlet_data->lod_nodes.empty())
			lod_nodes_offset = GlobalBufferCache::addLodNodeData(meshlet_data->lod_nodes.data(), meshlet_data->lod_nodes.size(), cmd_list);

		GlobalBufferCache::registerMeshOffsets(id, lod_groups_offset, lod_nodes_offset);
	}
}
