#include "pch.h"
#include "RayTracingScene.h"
#include "RHI/VkWrapper.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

void RayTracingScene::update()
{
	build_tlas();
}

void RayTracingScene::build_blas()
{
	size_t blas_size = 15000000;
	BufferDescription big_desc;
	big_desc.size = sizeof(Engine::Vertex) * blas_size;
	big_desc.useStagingBuffer = true;
	big_desc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	big_vertex_buffer = std::make_shared<Buffer>(big_desc);

	big_desc.size = sizeof(uint32_t) * blas_size;
	big_desc.useStagingBuffer = true;
	big_desc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	big_index_buffer = std::make_shared<Buffer>(big_desc);

	auto entities = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
	for (auto &entity_id : entities)
	{
		Entity entity(entity_id);
		auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
		for (auto &mesh_node : mesh_renderer.meshes)
		{
			auto mesh = mesh_node.getMesh();
			VkWrapper::copyBuffer(mesh->vertexBuffer->bufferHandle, big_vertex_buffer->bufferHandle, mesh->vertexBuffer->getSize(), big_vertex_buffer_last_offset);
			VkWrapper::copyBuffer(mesh->indexBuffer->bufferHandle, big_index_buffer->bufferHandle, mesh->indexBuffer->getSize(), big_index_buffer_last_offset);
			MeshOffset offset;
			offset.vertexBufferOffset = big_vertex_buffer_last_offset / sizeof(Engine::Vertex);
			offset.indexBufferOffset = big_index_buffer_last_offset / sizeof(uint32_t);
			mesh_offsets[mesh->id] = offset;
			big_vertex_buffer_last_offset += mesh->vertexBuffer->getSize();
			big_index_buffer_last_offset += mesh->indexBuffer->getSize();
		}
	}

	// Create BLAS for every node
	{

		if (!transform_buffer)
		{
			// Setup identity transform matrix
			VkTransformMatrixKHR transformMatrix = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f
			};
			// Create Transform buffer
			BufferDescription transformDesc;
			transformDesc.size = sizeof(transformMatrix);
			transformDesc.useStagingBuffer = true;
			transformDesc.bufferUsageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
			transformDesc.alignment = 16;

			transform_buffer = std::make_shared<Buffer>(transformDesc);
			transform_buffer->fill(&transformMatrix);
		}

		auto entities = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
		for (auto &entity_id : entities)
		{
			Entity entity(entity_id);
			auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
			for (auto &mesh_node : mesh_renderer.meshes)
			{
				auto mesh = mesh_node.getMesh();

				VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
				VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
				VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

				vertexBufferDeviceAddress.deviceAddress = VkWrapper::getBufferDeviceAddress(mesh->vertexBuffer->bufferHandle);
				indexBufferDeviceAddress.deviceAddress = VkWrapper::getBufferDeviceAddress(mesh->indexBuffer->bufferHandle);
				transformBufferDeviceAddress.deviceAddress = VkWrapper::getBufferDeviceAddress(transform_buffer->bufferHandle);

				VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
				triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				triangles.vertexData = vertexBufferDeviceAddress;
				triangles.maxVertex = mesh->vertices.size();
				triangles.vertexStride = sizeof(Engine::Vertex);
				triangles.indexType = VK_INDEX_TYPE_UINT32;
				triangles.indexData = indexBufferDeviceAddress;
				triangles.transformData.deviceAddress = 0;
				triangles.transformData.hostAddress = nullptr;
				triangles.transformData = transformBufferDeviceAddress;

				VkAccelerationStructureGeometryKHR acc_geometry{};
				acc_geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				acc_geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
				acc_geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
				acc_geometry.geometry.triangles = triangles;

				uint32_t numTriangles = mesh->indices.size() / 3;

				VkAccelerationStructureBuildRangeInfoKHR build_range_info{};
				build_range_info.primitiveCount = numTriangles;
				build_range_info.primitiveOffset = 0;
				build_range_info.firstVertex = 0;
				build_range_info.transformOffset = 0;

				blas_meshes.emplace(mesh->id, blas_meshes.size());

				bottomLevelAS.buildBLAS({acc_geometry}, {build_range_info}, {numTriangles});
			}
		}

		//?
	}
}

void RayTracingScene::build_tlas()
{
	// Create TLAS
	{
		obj_descs.clear();
		auto entities = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
		int object_id = 0;
		for (auto &entity_id : entities)
		{
			Entity entity(entity_id);
			auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
			int material_id = 0;
			for (auto &mesh_node : mesh_renderer.meshes)
			{
				auto mesh = mesh_node.getMesh();

				if (blas_meshes.find(mesh->id) == blas_meshes.end())
					continue;

				size_t blas_id = blas_meshes[mesh->id];
				topLevelAS.addInstance(bottomLevelAS.blases[blas_id].deviceAddress, entity.getWorldTransformMatrix(), object_id);

				auto &material = mesh_renderer.materials[material_id];
				ObjDesc obj_desc;
				obj_desc.color = material->albedo;
				obj_desc.vertexBufferOffset = mesh_offsets[mesh->id].vertexBufferOffset;
				obj_desc.indexBufferOffset = mesh_offsets[mesh->id].indexBufferOffset;
				obj_descs.push_back(obj_desc);

				if (material_id < mesh_renderer.materials.size() - 1)
					material_id++;
				object_id++;
			}
		}

		topLevelAS.buildTLAS(topLevelAS.handle != 0);
	}
}
