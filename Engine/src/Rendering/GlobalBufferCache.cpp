#include "pch.h"
#include "GlobalBufferCache.h"
#include "Core/Variables.h"
#include "RHI/DynamicRHI.h"

void GlobalBufferCache::GlobalBuffer::init(uint64_t initial_max_size, BufferUsage usage, const char *name, bool growable)
{
	this->initial_max_size = initial_max_size;
	buffer_usage = usage;
	debug_name = name;
	can_grow = growable;

	if (!can_grow && initial_max_size > 0)
		ensure_created(initial_max_size);
}

void GlobalBufferCache::GlobalBuffer::ensure_created(uint64_t minimum_size)
{
	if (buffer)
		return;

	uint64_t new_size = eastl::max(minimum_size, initial_max_size);

	BufferDescription desc;
	desc.size = new_size;
	desc.use_staging_buffer = true;
	desc.usage = buffer_usage;
	desc.storage_stride = sizeof(uint32_t);
	buffer = gDynamicRHI->createBuffer(desc);
	buffer->setDebugName(debug_name);

	max_size = new_size;
	free_ranges.push_back({0, new_size});
}

uint64_t GlobalBufferCache::GlobalBuffer::allocate_range(uint64_t needed_size, RHICommandList *cmd_list)
{
	ensure_created(needed_size);

	auto find_first_fit = [&]()
	{
		for (auto it = free_ranges.begin(); it != free_ranges.end(); ++it)
		{
			if (it->size >= needed_size)
				return it;
		}
		return free_ranges.end();
	};

	auto fit = find_first_fit();

	if (fit == free_ranges.end())
	{
		if (!can_grow || !cmd_list)
			return UINT64_MAX;

		uint64_t new_size = eastl::max(max_size + needed_size, max_size * 3);
		CORE_INFO("[GlobalBufferCache] {} buffer grow: {} -> {} bytes\n", debug_name, max_size, new_size);

		BufferDescription desc;
		desc.size = new_size;
		desc.use_staging_buffer = true;
		desc.usage = buffer_usage;
		desc.storage_stride = sizeof(uint32_t);
		RHIBufferRef new_buffer = gDynamicRHI->createBuffer(desc);
		new_buffer->setDebugName(debug_name);

		cmd_list->copyBuffer(buffer, new_buffer, 0, 0, max_size);

		buffer = new_buffer;
		max_size = new_size;
		free_ranges.push_back({max_size, new_size - max_size});

		fit = find_first_fit();
		if (fit == free_ranges.end())
			return UINT64_MAX;
	}

	uint64_t offset = fit->offset;

	if (fit->size > needed_size)
	{
		fit->offset += needed_size;
		fit->size -= needed_size;
	} else
	{
		free_ranges.erase(fit);
	}

	return offset;
}

uint64_t GlobalBufferCache::GlobalBuffer::add(const void *cpu_data, uint64_t size, RHICommandList *cmd_list)
{
	uint64_t offset = allocate_range(size, cmd_list);
	if (offset == UINT64_MAX)
		return UINT64_MAX;

	BufferDescription staging_desc;
	staging_desc.size = size;
	staging_desc.usage = BufferUsage::STAGING_BUFFER;
	staging_desc.use_staging_buffer = false;
	staging_desc.storage_stride = sizeof(uint32_t);
	RHIBufferRef staging = gDynamicRHI->createBuffer(staging_desc);
	staging->fill(cpu_data);

	cmd_list->copyBuffer(staging, buffer, 0, offset, size);
	buffer->transitState(ResourceState::SHADER_RESOURCE);
	return offset;
}

uint64_t GlobalBufferCache::GlobalBuffer::add(RHIBuffer *src_buffer, uint64_t buffer_offset, uint64_t buffer_size, RHICommandList *cmd_list)
{
	uint64_t offset = allocate_range(buffer_size, cmd_list);
	if (offset == UINT64_MAX)
		return UINT64_MAX;

	cmd_list->copyBuffer(src_buffer, buffer, buffer_offset, offset, buffer_size);
	buffer->transitState(ResourceState::SHADER_RESOURCE);
	return offset;
}

void GlobalBufferCache::GlobalBuffer::remove(uint64_t offset, uint64_t size)
{
	FreeRange range = {offset, size};

	auto it = eastl::lower_bound(free_ranges.begin(), free_ranges.end(), range,
		[](const FreeRange &a, const FreeRange &b) { return a.offset < b.offset; });
	it = free_ranges.insert(it, range);

	auto next = it + 1;
	if (next != free_ranges.end() && it->offset + it->size == next->offset)
	{
		it->size += next->size;
		free_ranges.erase(next);
	}
	if (it != free_ranges.begin())
	{
		auto prev = it - 1;
		if (prev->offset + prev->size == it->offset)
		{
			prev->size += it->size;
			free_ranges.erase(it);
		}
	}
}

uint64_t GlobalBufferCache::GlobalBuffer::getUsedSize() const
{
	uint64_t free_bytes = 0;
	for (const FreeRange &range : free_ranges)
		free_bytes += range.size;
	return max_size - free_bytes;
}

void GlobalBufferCache::shutdown()
{
	geometry = GlobalBuffer{};
	lod_groups = GlobalBuffer{};
	lod_nodes = GlobalBuffer{};
	mesh_offsets.clear();
}

void GlobalBufferCache::registerMeshOffsets(size_t mesh_id, uint64_t lod_groups_offset, uint64_t lod_nodes_offset)
{
	mesh_offsets[mesh_id] = {lod_groups_offset, lod_nodes_offset};
}

GlobalBufferCache::MeshGlobalOffsets GlobalBufferCache::getMeshOffsets(size_t mesh_id)
{
	return mesh_offsets.at(mesh_id);
}

uint64_t GlobalBufferCache::addMeshletLodGroupData(LODGroup *data, uint32_t count, RHICommandList *cmd_list)
{
	if (!lod_groups.isInitialized())
		lod_groups.init(3'000'000 * sizeof(LODGroup), BufferUsage::SHADER_READ_BUFFER, "LOD Groups");
	uint64_t byte_offset = lod_groups.add(data, count * sizeof(LODGroup), cmd_list);
	return byte_offset == UINT64_MAX ? UINT64_MAX : byte_offset / sizeof(LODGroup);
}

uint64_t GlobalBufferCache::addLodNodeData(LodNode *data, uint32_t count, RHICommandList *cmd_list)
{
	if (!lod_nodes.isInitialized())
		lod_nodes.init(3'000'000 * sizeof(LodNode), BufferUsage::SHADER_READ_BUFFER, "LOD Nodes");
	uint64_t byte_offset = lod_nodes.add(data, count * sizeof(LodNode), cmd_list);
	return byte_offset == UINT64_MAX ? UINT64_MAX : byte_offset / sizeof(LodNode);
}

uint64_t GlobalBufferCache::addMeshletGeometryData(RHIBuffer *source, uint64_t source_offset, uint32_t size, RHICommandList *cmd_list)
{
	if (!geometry.isInitialized())
		geometry.init(1024ull * 1024 * 1024 * 1.5, BufferUsage::VERTEX_BUFFER | BufferUsage::INDEX_BUFFER | BufferUsage::SHADER_READ_BUFFER, "Global Meshlet Geometry", false);
	return geometry.add(source, source_offset, size, cmd_list);
}

void GlobalBufferCache::removeMeshletGeometryData(uint64_t offset, uint64_t size)
{
	geometry.remove(offset, size);
}
