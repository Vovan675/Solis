#include "pch.h"
#include "RayTracingScene.h"

void RayTracingScene::setInstance(uint32_t slot, Engine::Mesh *mesh, const glm::mat4 &transform)
{
	if (!mesh->indexed)
		return;
	instances[slot] = {mesh, transform};
}

void RayTracingScene::removeInstance(uint32_t slot)
{
	instances.erase(slot);
}

void RayTracingScene::invalidateMesh(Engine::Mesh *mesh)
{
	blases.erase(mesh);
}

RHIBottomLevelAccelerationStructureRef RayTracingScene::ensure_blas(Engine::Mesh *mesh)
{
	auto it = blases.find(mesh);
	if (it != blases.end())
		return it->second;

	eastl::vector<RayTracingGeometry> geometries;
	RayTracingGeometry &geometry = geometries.emplace_back();
	geometry.vertex_buffer = mesh->indexed->vertex_buffer;
	geometry.vertex_buffer_offset = 0;
	geometry.vertex_buffer_stride = sizeof(Engine::Vertex);
	geometry.vertex_count = mesh->indexed->vertices.size();
	geometry.vertex_format = FORMAT_R32G32B32_SFLOAT;

	geometry.index_buffer = mesh->indexed->index_buffer;
	geometry.index_buffer_offset = 0;
	geometry.index_count = mesh->indexed->indices.size();
	geometry.index_format = FORMAT_R32_UINT;

	auto blas = gDynamicRHI->createBottomLevelAccelerationStructure();
	blas->build(geometries);
	blases[mesh] = blas;
	return blas;
}

void RayTracingScene::update(Camera *camera)
{
	eastl::vector<RayTracingInstance> rt_instances;
	rt_instances.reserve(instances.size());
	for (auto &[slot, entry] : instances)
	{
		RayTracingInstance &instance = rt_instances.emplace_back();
		instance.blas = ensure_blas(entry.mesh);
		instance.transform = entry.transform;
		instance.instance_id = slot;
		instance.instance_mask = 0xFF;
		instance.instance_contribution_to_hit_group_index = 0;
	}

	topLevelAS->build(false, rt_instances);
}
