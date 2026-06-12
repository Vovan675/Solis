#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "ShaderStructs.h"
#include "Renderer.h"
#include "FrameGraph/FrameGraph.h"

class GeometryStreaming
{
public:
	void init();
	void registerMesh(Engine::Mesh *mesh, const Engine::MeshletFileView &file_view);
	void unregisterMesh(Engine::Mesh *mesh);
	void update();
	void importBuffers(FrameGraph &frame_graph);
	void addAgeFilterAndReadbackPasses(FrameGraph &frame_graph);

	uint32_t getMeshResidencyOffset(Engine::Mesh *mesh) const { return registered_meshes.at(mesh).group_residency_offset; }

	struct Stats
	{
		uint64_t total_loads = 0;
		uint64_t total_unloads = 0;
		uint64_t total_bytes_loaded = 0;
		uint64_t total_bytes_unloaded = 0;

		uint32_t loads_last_frame = 0;
		uint32_t unloads_last_frame = 0;
		uint32_t bytes_loaded_last_frame = 0;
		uint32_t bytes_unloaded_last_frame = 0;

		uint32_t resident_groups = 0;
		uint32_t total_groups = 0;
		uint32_t registered_mesh_count = 0;
		uint32_t pending_load_queue_size = 0;
		uint32_t pending_frees_count = 0;

		void addLoad(uint64_t size)
		{
			total_loads++;
			loads_last_frame++;
			total_bytes_loaded += size;
			bytes_loaded_last_frame += (uint32_t)size;
			resident_groups++;
		}

		void addUnload(uint64_t size)
		{
			total_unloads++;
			unloads_last_frame++;
			total_bytes_unloaded += size;
			bytes_unloaded_last_frame += (uint32_t)size;
			if (resident_groups > 0)
				resident_groups--;
		}
	};
	const Stats &getStats() const { return stats; }

private:
	struct RegisteredMesh
	{
		Engine::MeshletFileView file_view;
		uint32_t group_residency_offset;
	};
	struct PendingFree
	{
		uint64_t offset;
		uint64_t size;
		uint64_t frame;
	};

	struct FreeRange
	{
		uint32_t offset;
		uint32_t count;
	};
	eastl::vector<FreeRange> free_ranges;

	void process_gpu_requests(int frame);
	void process_deferred_frees();
	eastl::vector<uint32_t> upload_pending_groups(RHICommandList *cmd_list);
	uint32_t upload_group_data(Engine::Mesh *mesh, uint32_t local_group_id, const Engine::MeshletFileView &file_view, RHICommandList *cmd_list);
	uint32_t allocate_residency_range(uint32_t count);

	eastl::unordered_map<Engine::Mesh *, RegisteredMesh> registered_meshes;
	eastl::vector<Engine::Mesh *> flat_to_mesh;
	eastl::hash_map<uint32_t, uint32_t> load_queued;
	eastl::vector<PendingFree> pending_frees;

	bool is_residency_dirty = false;
	Stats stats;
	eastl::vector<GroupResidency> group_residency;
	RHIBufferRef group_residency_gpu;
	RHIBufferRef group_ages_gpu;

	RHIBufferRef stream_requests_gpu;
	RHIBufferRef stream_requests_readback[MAX_FRAMES_IN_FLIGHT];
};
