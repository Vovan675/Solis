#include "pch.h"
#include "RayTracingScene.h"

void RayTracingScene::setInstance(uint32_t slot, Engine::Mesh *mesh, const glm::mat4 &transform)
{
	if (!mesh->indexed)
		return;
	InstanceEntry &entry = instances[slot];
	entry.mesh = mesh;
	entry.transform = transform;
}

void RayTracingScene::setInstanceFlags(uint32_t slot, RayTracingInstanceFlags instance_flags)
{
	instances[slot].instance_flags = instance_flags;
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
	geometry.vertex_format = Format::R32G32B32_SFLOAT;

	geometry.index_buffer = mesh->indexed->index_buffer;
	geometry.index_buffer_offset = 0;
	geometry.index_count = mesh->indexed->indices.size();
	geometry.index_format = IndexFormat::UINT32;

	auto blas = gDynamicRHI->createBottomLevelAccelerationStructure();
	blas->build(geometries);
	blases[mesh] = blas;
	return blas;
}

void RayTracingScene::update()
{
	eastl::vector<RayTracingInstance> rt_instances;
	rt_instances.reserve(instances.size());
	for (auto &[slot, entry] : instances)
	{
		if (!entry.mesh)
			continue;

		RayTracingInstance &instance = rt_instances.emplace_back();
		instance.blas = ensure_blas(entry.mesh);
		instance.transform = entry.transform;
		instance.instance_id = slot;
		instance.instance_mask = 0xFF;
		instance.instance_contribution_to_hit_group_index = 0;
		instance.instance_flags = entry.instance_flags;
		if (!GFXOPTIONS(ray_traced_alpha_test))
			instance.instance_flags |= RayTracingInstanceFlags::FORCE_OPAQUE;
	}

	topLevelAS->build(rt_instances);
}
