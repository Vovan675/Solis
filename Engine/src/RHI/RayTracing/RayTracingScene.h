#pragma once
#include "Core/Core.h"
#include "Scene/Scene.h"
#include "RHI/RHIAccelerationStructure.h"
#include "Rendering/Mesh.h"

class RayTracingScene : public RefCounted
{
public:
	RayTracingScene()
	{
		topLevelAS = gDynamicRHI->createTopLevelAccelerationStructure();
	}

	void setInstance(uint32_t slot, Engine::Mesh *mesh, const glm::mat4 &transform);
	void removeInstance(uint32_t slot);
	void setInstanceFlags(uint32_t slot, RayTracingInstanceFlags instance_flags);
	void setInstanceMask(uint32_t slot, uint8_t instance_mask);
	void invalidateMesh(Engine::Mesh *mesh);
	void update();

	RHITopLevelAccelerationStructureRef getTopLevelAS() { return topLevelAS; }

private:
	RHIBottomLevelAccelerationStructureRef ensure_blas(Engine::Mesh *mesh);

	struct InstanceEntry
	{
		Engine::Mesh *mesh = nullptr;
		glm::mat4 transform;
		RayTracingInstanceFlags instance_flags = RayTracingInstanceFlags::NONE;
		uint8_t instance_mask = 0xFF;
	};

	eastl::hash_map<uint32_t, InstanceEntry> instances;
	eastl::unordered_map<Engine::Mesh *, RHIBottomLevelAccelerationStructureRef> blases;
	RHITopLevelAccelerationStructureRef topLevelAS;
};
