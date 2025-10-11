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
	RHIBufferRef getBigVertexBuffer() { return big_vertex_buffer; }
	RHIBufferRef getBigIndexBuffer() { return big_index_buffer; }

	struct ObjDesc
	{
		glm::vec4 color;
		uint32_t vertexBufferOffset;
		uint32_t indexBufferOffset;
	};

	eastl::vector<ObjDesc> &getObjDescs() { return obj_descs; }
private:
	void build_blas();
	void build_tlas();

	Scene *scene;

	struct MeshOffset
	{
		int vertexBufferOffset;
		int indexBufferOffset;
	};
	eastl::unordered_map<size_t, MeshOffset> mesh_offsets = {};

	eastl::unordered_map<Engine::Mesh *, RHIBottomLevelAccelerationStructureRef> blases;
	RHITopLevelAccelerationStructureRef topLevelAS;

	eastl::unordered_map<size_t, size_t> blas_meshes = {};


	RHIBufferRef transform_buffer;

	RHIBufferRef big_vertex_buffer;
	RHIBufferRef big_index_buffer;
	uint64_t big_vertex_buffer_last_offset = 0;
	uint64_t big_index_buffer_last_offset = 0;

	eastl::vector<ObjDesc> obj_descs = {};
};
