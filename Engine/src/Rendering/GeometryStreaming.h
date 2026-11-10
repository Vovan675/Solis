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
	void addReadbackPass(FrameGraph &frame_graph);

	uint32_t getMeshPageTableOffset(Engine::Mesh *mesh) const { return registered_meshes.at(mesh).page_table_offset; }

	struct Stats
	{
		uint32_t pool_slots = 0;
		uint32_t used_slots = 0;
		uint32_t needed_pages = 0;
		uint32_t missing_pages = 0;
		uint32_t loaded_pages = 0;

		uint32_t registered_mesh_count = 0;
		uint64_t root_bytes = 0;
	};
	const Stats &getStats() const { return stats; }

private:
	struct RegisteredMesh
	{
		Engine::MeshletFileView file_view;
		uint32_t page_table_offset;
		uint64_t root_page_offset;
		uint32_t root_page_bytes;
	};

	struct FreeRange
	{
		uint32_t offset;
		uint32_t count;
	};

	void create_page_buffers(uint32_t page_capacity);
	void process_gpu_requests(int frame);
	void set_slot_page(uint32_t slot, uint32_t page);
	void upload_page(uint32_t page, uint32_t slot);
	uint32_t allocate_page_table_range(uint32_t count);

	eastl::unordered_map<Engine::Mesh *, RegisteredMesh> registered_meshes;

	// Page table
	eastl::vector<uint32_t> page_table;
	eastl::vector<Engine::Mesh *> flat_to_mesh;
	eastl::vector<FreeRange> free_ranges;

	RHIBufferRef page_table_gpu;
	bool is_residency_dirty = false;
	eastl::vector<uint32_t> dirty_pages;

	// Pool of resident pages
	uint64_t pool_offset = 0;
	eastl::vector<uint32_t> slot_to_page; // the page in every slot

	// GPU feedback
	RHIBufferRef stream_requests_gpu;
	RHIBufferRef stream_requests_readback[MAX_FRAMES_IN_FLIGHT];
	RHIBufferRef page_frames_gpu; // last frame when page was needed
	RHIBufferRef page_frames_readback[MAX_FRAMES_IN_FLIGHT];
	uint64_t first_readback_frame = MAX_FRAMES_IN_FLIGHT;

	Stats stats;
};
