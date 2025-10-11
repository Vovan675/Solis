#include "pch.h"
#include "RayTracingScene.h"
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
	big_desc.usage = VERTEX_BUFFER | UAV_BUFFER;
	big_desc.useStagingBuffer = true;
	big_vertex_buffer = gDynamicRHI->createBuffer(big_desc);

	big_desc.size = sizeof(uint32_t) * blas_size;
	big_desc.useStagingBuffer = true;
	big_desc.usage = INDEX_BUFFER | UAV_BUFFER;
	big_index_buffer = gDynamicRHI->createBuffer(big_desc);

	auto components = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
	for (auto &&[entity, transform_buffer, mesh_renderer]: components.each())
	{
		for (auto &mesh_node : mesh_renderer.meshes)
		{
			auto mesh = mesh_node.getMesh();

			RHICommandList *copy_cmd_list = gDynamicRHI->getCmdListCopy();
			copy_cmd_list->open();
			copy_cmd_list->copyBuffer(mesh->vertexBuffer, big_vertex_buffer, 0, big_vertex_buffer_last_offset, mesh->vertexBuffer->getSize());
			copy_cmd_list->close();
			gDynamicRHI->getCmdQueueCopy()->execute(copy_cmd_list);
			// Wait queue
			auto last_fence = gDynamicRHI->getCmdQueueCopy()->getLastFenceValue();
			gDynamicRHI->getCmdQueueCopy()->signal(last_fence + 1);
			gDynamicRHI->getCmdQueueCopy()->wait(last_fence + 1);

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
			transformDesc.usage = ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;
			transformDesc.alignment = 16;

			transform_buffer = gDynamicRHI->createBuffer(transformDesc);
			transform_buffer->fill(&transformMatrix);
		}

		auto entities = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
		for (auto &entity_id : entities)
		{
			Entity entity(entity_id);
			auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
			for (auto &mesh_node : mesh_renderer.meshes)
			{
				auto *mesh = mesh_node.getMesh();

				blas_meshes.emplace(mesh->id, blas_meshes.size());
				eastl::vector<RayTracingGeometry> geometries;
				RayTracingGeometry &geometry = geometries.emplace_back();

				geometry.vertex_buffer = mesh->vertexBuffer;
				geometry.vertex_buffer_offset = 0;
				geometry.vertex_buffer_stride = sizeof(Engine::Vertex);
				geometry.vertex_count = mesh->vertices.size();
				geometry.vertex_format = FORMAT_R32G32B32_SFLOAT;

				geometry.index_buffer = mesh->indexBuffer;
				geometry.index_buffer_offset = 0;
				geometry.index_count = mesh->indices.size();
				geometry.index_format = FORMAT_R32_UINT;

				auto blas = gDynamicRHI->createBottomLevelAccelerationStructure();
				blas->build(geometries);
				blases[mesh] = eastl::move(blas);
			}
		}
	}
}

void RayTracingScene::build_tlas()
{
	// Create TLAS
	{
		eastl::vector<RayTracingInstance> instances;
		obj_descs.clear();
		auto components = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
		instances.reserve(components.size_hint());
		int object_id = 0;
		for (auto &&[entity, transform, mesh_renderer]: components.each())
		{
			int material_id = 0;
			for (auto &mesh_node : mesh_renderer.meshes)
			{
				auto mesh = mesh_node.getMesh();

				if (blas_meshes.find(mesh->id) == blas_meshes.end())
					continue;

				size_t blas_id = blas_meshes[mesh->id];
				
				RayTracingInstance &instance = instances.emplace_back();
				instance.blas = blases[mesh];
				instance.transform = transform.getWorldTransform();
				instance.instance_id = object_id;
				instance.instance_mask = 0xFF;
				instance.instance_contribution_to_hit_group_index = 0;

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

		topLevelAS->build(false, instances);
	}
}
