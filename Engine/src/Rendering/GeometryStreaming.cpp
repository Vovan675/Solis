#include "pch.h"
#include "GeometryStreaming.h"
#include "Core/Platform.h"
#include "Scene/Components.h"
#include "GlobalBufferCache.h"
#include "Assets/MeshFormat.h"
#include "Rendering/UploadManager.h"
#include "FrameGraph/FrameGraph.h"
#include "meshoptimizer.h"

namespace
{
constexpr uint32_t POOL_PAGE_COUNT = 8192; // 128kb * 8193 = 1 GB
constexpr uint32_t MAX_PAGES_PER_FRAME = 64;
constexpr uint32_t INITIAL_PAGE_CAPACITY = 65536;
constexpr uint32_t EMPTY_SLOT = UINT32_MAX;

struct StreamRequestsBufferLayout
{
	uint32_t load_count;
	uint32_t load_indices[MAX_STREAMING_REQUESTS];
};
constexpr uint32_t STREAM_REQUESTS_BUFFER_SIZE = sizeof(StreamRequestsBufferLayout);

RHIBufferRef create_storage_buffer(uint32_t size, BufferUsage usage_flags, BufferMemory memory, const char *debug_name)
{
	BufferDescription desc;
	desc.size = size;
	desc.usage_flags = usage_flags;
	desc.memory = memory;
	desc.stride = sizeof(uint32_t);
	RHIBufferRef buffer = gDynamicRHI->createBuffer(desc);
	buffer->setDebugName(debug_name);
	return buffer;
}

// Writes page [Meshlets | Vertices | Triangles] into dst
void fill_page(uint8_t *dst, const Engine::MeshletGeometry::Page &page, const Engine::MeshletFileView &file_view)
{
	const uint8_t *src = file_view.pages_data + page.file_offset;

	// Copy everything up to triangles indices
	memcpy(dst, src, page.getIndicesOffset());

	// Decode every meshlet [uint8 triangle count][uint16 size][meshopt_encodeMeshlet bytes] using meshopt_decodeMeshlet
	const uint8_t *src_tris = src + page.getIndicesOffset();
	uint8_t *dst_tris = dst + page.getIndicesOffset();
	uint32_t decoded_tris[256 * 3 / 4]; // the decoder writes whole aligned dwords
	while (src_tris < src + page.getFileBytes())
	{
		uint8_t triangle_count = *src_tris;
		src_tris += sizeof(triangle_count);

		uint16_t encoded_bytes;
		memcpy(&encoded_bytes, src_tris, sizeof(encoded_bytes));
		src_tris += sizeof(encoded_bytes);

		int result = meshopt_decodeMeshlet(nullptr, 0, 4, decoded_tris, triangle_count, 3, src_tris, encoded_bytes);
		ENGINE_ASSERT(result == 0);
		src_tris += encoded_bytes;

		uint32_t index_count = triangle_count * 3;
		memcpy(dst_tris, decoded_tris, index_count);
		dst_tris += index_count;
	}
}
}

void GeometryStreaming::set_slot_page(uint32_t slot, uint32_t page)
{
	uint32_t old_page = slot_to_page[slot];
	if (old_page != EMPTY_SLOT)
	{
		page_table[old_page] = MESHLET_PAGE_NOT_RESIDENT;
		dirty_pages.push_back(old_page);
		stats.used_slots--;
	}

	slot_to_page[slot] = page;
	if (page != EMPTY_SLOT)
	{
		page_table[page] = pool_offset + slot * MESHLET_PAGE_SIZE;
		dirty_pages.push_back(page);
		stats.used_slots++;
	}
}

void GeometryStreaming::upload_page(uint32_t page, uint32_t slot)
{
	PROFILE_CPU_FUNCTION();
	Engine::Mesh *mesh = flat_to_mesh[page];
	const RegisteredMesh &reg = registered_meshes.at(mesh);
	const Engine::MeshletGeometry::Page &page_info = mesh->meshlet_data->pages[page - reg.page_table_offset];

	UploadManager::StagedRange range = gUploadManager->stage(page_info.getGpuBytes());
	fill_page(range.data, page_info, reg.file_view);
	set_slot_page(slot, page);
	GlobalBufferCache::writeMeshletGeometryData(page_table[page], range.buffer, range.offset, page_info.getGpuBytes(), gDynamicRHI->getCmdList());
}

void GeometryStreaming::create_page_buffers(uint32_t page_capacity)
{
	uint32_t size = page_capacity * sizeof(uint32_t);
	page_table_gpu = create_storage_buffer(size, BufferUsage::SHADER_RESOURCE, BufferMemory::GPU, "Meshlet Page Table");
	page_frames_gpu = create_storage_buffer(size, BufferUsage::UNORDERED_ACCESS, BufferMemory::GPU, "Meshlet Page Frames");
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		page_frames_readback[i] = create_storage_buffer(size, BufferUsage::NONE, BufferMemory::READBACK, "Meshlet Page Frames Readback");
}

void GeometryStreaming::init()
{
	stream_requests_gpu = create_storage_buffer(STREAM_REQUESTS_BUFFER_SIZE, BufferUsage::UNORDERED_ACCESS, BufferMemory::GPU, "Streaming Requests GPU");
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		stream_requests_readback[i] = create_storage_buffer(STREAM_REQUESTS_BUFFER_SIZE, BufferUsage::NONE, BufferMemory::READBACK, "Streaming Requests Readback");

	create_page_buffers(INITIAL_PAGE_CAPACITY);
}

// Free list range allocation
uint32_t GeometryStreaming::allocate_page_table_range(uint32_t count)
{
	for (int i = 0; i < free_ranges.size(); i++)
	{
		if (free_ranges[i].count < count)
			continue;
		uint32_t offset = free_ranges[i].offset;
		if (free_ranges[i].count == count)
		{
			free_ranges.erase(free_ranges.begin() + i);
		} else
		{
			free_ranges[i].offset += count;
			free_ranges[i].count -= count;
		}
		return offset;
	}

	uint32_t offset = page_table.size();
	page_table.resize(offset + count);
	flat_to_mesh.resize(offset + count, nullptr);
	return offset;
}

void GeometryStreaming::registerMesh(Engine::Mesh *mesh, const Engine::MeshletFileView &file_view)
{
	if (!mesh->useMeshlets())
		return;
	if (registered_meshes.contains(mesh))
		return;

	// Allocate full size for pool at first need
	if (slot_to_page.empty())
	{
		pool_offset = GlobalBufferCache::allocateMeshletGeometryData(POOL_PAGE_COUNT * MESHLET_PAGE_SIZE);
		slot_to_page.resize(POOL_PAGE_COUNT, EMPTY_SLOT);
		stats.pool_slots = POOL_PAGE_COUNT;
	}

	const eastl::vector<Engine::MeshletGeometry::Page> &pages = mesh->meshlet_data->pages;
	uint32_t base = allocate_page_table_range(pages.size());

	RegisteredMesh &reg = registered_meshes[mesh];
	reg.file_view = file_view;
	reg.page_table_offset = base;

	for (int i = 0; i < pages.size(); i++)
	{
		flat_to_mesh[base + i] = mesh;
		page_table[base + i] = MESHLET_PAGE_NOT_RESIDENT;
	}

	// Root page
	reg.root_page_bytes = Math::alignedSize(pages[0].getGpuBytes(), 4u);

	eastl::vector<uint8_t> root_page(reg.root_page_bytes);
	fill_page(root_page.data(), pages[0], file_view);
	reg.root_page_offset = GlobalBufferCache::addMeshletGeometryData(root_page.data(), reg.root_page_bytes, gDynamicRHI->getCmdList()); // Allocate as separate geometry data (not inside streaming pool)

	page_table[base] = reg.root_page_offset;

	is_residency_dirty = true;
	stats.registered_mesh_count = registered_meshes.size();
	stats.root_bytes += reg.root_page_bytes;
}

void GeometryStreaming::unregisterMesh(Engine::Mesh *mesh)
{
	auto it = registered_meshes.find(mesh);
	if (it == registered_meshes.end())
		return;

	uint32_t base = it->second.page_table_offset;
	uint32_t pages_count = mesh->meshlet_data->pages.size();

	for (int i = 1; i < pages_count; i++)
	{
		uint32_t flat_index = base + i;
		if (page_table[flat_index] != MESHLET_PAGE_NOT_RESIDENT)
			set_slot_page((page_table[flat_index] - pool_offset) / MESHLET_PAGE_SIZE, EMPTY_SLOT);
	}
	GlobalBufferCache::removeMeshletGeometryData(it->second.root_page_offset, it->second.root_page_bytes);
	stats.root_bytes -= it->second.root_page_bytes;
	eastl::fill(flat_to_mesh.begin() + base, flat_to_mesh.begin() + base + pages_count, nullptr);

	free_ranges.push_back({base, pages_count});
	registered_meshes.erase(it);
	stats.registered_mesh_count = registered_meshes.size();
}

void GeometryStreaming::update()
{
	PROFILE_CPU_FUNCTION();
	if (gDynamicRHI->getFrame() >= first_readback_frame)
		process_gpu_requests(gDynamicRHI->getFrameInFlight());

	uint32_t page_capacity = page_table_gpu->getSize() / sizeof(uint32_t);
	if (page_capacity < page_table.size())
	{
		while (page_capacity < page_table.size())
			page_capacity *= 2;
		create_page_buffers(page_capacity);
		gDynamicRHI->getCmdList()->clearBuffer(page_frames_gpu, 0);
		first_readback_frame = gDynamicRHI->getFrame() + MAX_FRAMES_IN_FLIGHT;
		is_residency_dirty = true;
	}

	if (is_residency_dirty)
	{
		gUploadManager->queueUpload(GFXRID(PageTableBuffer), 0, page_table.data(), page_table.size() * sizeof(uint32_t));
		is_residency_dirty = false;
	} else
	{
		for (uint32_t page : dirty_pages)
			gUploadManager->queueUpload(GFXRID(PageTableBuffer), page * sizeof(uint32_t), &page_table[page], sizeof(uint32_t));
	}
	dirty_pages.clear();

	uint32_t zero = 0;
	gUploadManager->queueUpload(GFXRID(StreamRequestsBuffer), 0, &zero, sizeof(zero));
}

void GeometryStreaming::importBuffers(FrameGraph &frame_graph)
{
	frame_graph.importBuffer(GFXRID(PageTableBuffer), page_table_gpu);
	frame_graph.importBuffer(GFXRID(StreamRequestsBuffer), stream_requests_gpu);
	frame_graph.importBuffer(GFXRID(PageFramesBuffer), page_frames_gpu);
}

void GeometryStreaming::addReadbackPass(FrameGraph &frame_graph)
{
	RHIBufferRef requests = stream_requests_gpu;
	RHIBufferRef requests_readback = stream_requests_readback[gDynamicRHI->getFrameInFlight()];
	RHIBufferRef page_frames = page_frames_gpu;
	RHIBufferRef frames_readback = page_frames_readback[gDynamicRHI->getFrameInFlight()];

	frame_graph.addCallbackPass("Streaming Readback Copy",
	[](RenderPassBuilder &builder) { builder.setSideEffect(true); },
	[=](const RenderPassResources &, RHICommandList *cmd_list)
	{
		cmd_list->copyBuffer(requests, requests_readback, 0, 0, STREAM_REQUESTS_BUFFER_SIZE);
		cmd_list->copyBuffer(page_frames, frames_readback, 0, 0, page_frames->getSize());
	});
}

void GeometryStreaming::process_gpu_requests(int frame)
{
	PROFILE_CPU_FUNCTION();

	void *mapped_requests;
	stream_requests_readback[frame]->map(&mapped_requests);
	const StreamRequestsBufferLayout *requests = (const StreamRequestsBufferLayout *)mapped_requests;

	void *mapped_frames;
	page_frames_readback[frame]->map(&mapped_frames);
	const uint32_t *page_frames = (const uint32_t *)mapped_frames;

	uint32_t readback_frame = gDynamicRHI->getFrame() - MAX_FRAMES_IN_FLIGHT;
	auto get_slot_frame = [&](uint32_t slot) { return slot_to_page[slot] == EMPTY_SLOT ? 0u : page_frames[slot_to_page[slot]]; };

	// Page is from coarase to detailed, sort it for loading parents first
	eastl::vector<uint32_t> pages_to_load;
	uint32_t load_count = eastl::min(requests->load_count, MAX_STREAMING_REQUESTS);
	for (int i = 0; i < load_count; i++)
	{
		uint32_t flat_index = requests->load_indices[i];
		if (flat_to_mesh[flat_index] && page_table[flat_index] == MESHLET_PAGE_NOT_RESIDENT)
			pages_to_load.push_back(flat_index);
	}
	eastl::sort(pages_to_load.begin(), pages_to_load.end());

	// Get slots that could be reuploaded
	eastl::vector<uint32_t> slots;
	for (int slot = 0; slot < slot_to_page.size(); slot++)
	{
		// Use only slats that not touched at readback frame
		if (get_slot_frame(slot) < readback_frame)
			slots.push_back(slot);
	}

	// How much pages we can upload = min(how_much_pages_requested, how_much_slots_unused)
	uint32_t upload_count = eastl::min(eastl::min((uint32_t)pages_to_load.size(), (uint32_t)slots.size()), MAX_PAGES_PER_FRAME);

	// Sort first `upload_count` slots to most old used or more detailed
	eastl::partial_sort(slots.begin(), slots.begin() + upload_count, slots.end(), [&](uint32_t slot_a, uint32_t slot_b)
	{
		uint32_t frame_a = get_slot_frame(slot_a);
		uint32_t frame_b = get_slot_frame(slot_b);
		if (frame_a != frame_b)
			return frame_a < frame_b; // LRU eviction
		else
			return slot_to_page[slot_a] > slot_to_page[slot_b]; // Evict finer detailed
	});

	for (int i = 0; i < upload_count; i++)
		upload_page(pages_to_load[i], slots[i]);

	stats.needed_pages = slot_to_page.size() - slots.size() + pages_to_load.size();
	stats.missing_pages = pages_to_load.size();
	stats.loaded_pages = upload_count;

	stream_requests_readback[frame]->unmap();
	page_frames_readback[frame]->unmap();
}
