#pragma once
#include "Core/Core.h"
#include "Scene/Scene.h"
#include "RHI/RHIAccelerationStructure.h"
#include "Rendering/Mesh.h"

class RayTracingScene : public RefCounted
{
public:
	RayTracingScene(Scene *scene): scene(scene)
	{
		build_blas();
		topLevelAS = gDynamicRHI->createTopLevelAccelerationStructure();
	}

	void update();

	RHITopLevelAccelerationStructureRef getTopLevelAS() { return topLevelAS; }

	struct ObjDesc
	{
		glm::vec4 color;
		uint32_t vertexBufferOffset;
		uint32_t indexBufferOffset;
	};

private:
	void build_blas();
	void build_tlas();

	Scene *scene;

	eastl::unordered_map<Engine::Mesh *, RHIBottomLevelAccelerationStructureRef> blases;
	RHITopLevelAccelerationStructureRef topLevelAS;

	eastl::unordered_map<size_t, size_t> blas_meshes = {};

	RHIBufferRef transform_buffer;
};
