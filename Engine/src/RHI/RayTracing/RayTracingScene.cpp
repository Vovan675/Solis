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
		auto components = Scene::getCurrentScene()->getEntitiesWith<TransformComponent, MeshRendererComponent>();
		instances.reserve(components.size_hint());
		int object_id = 0;
		for (auto &&[entity, transform, mesh_renderer]: components.each())
		{
			int material_id = 0;
			for (auto &mesh_node : mesh_renderer.meshes)
			{
				auto mesh = mesh_node.getMesh();
				if (mesh == nullptr)
					continue;

				if (blas_meshes.find(mesh->id) == blas_meshes.end())
				{
					CORE_ERROR("Blas Instance not found for mesh");
					continue;
				}

				size_t blas_id = blas_meshes[mesh->id];
				
				RayTracingInstance &instance = instances.emplace_back();
				instance.blas = blases[mesh];
				instance.transform = transform.getWorldTransform();
				instance.instance_id = object_id;
				instance.instance_mask = 0xFF;
				instance.instance_contribution_to_hit_group_index = 0;

				object_id++;
			}
		}

		topLevelAS->build(false, instances);
	}
}
