#include "pch.h"
#include "GeometryStreaming.h"
#include "Core/Platform.h"
#include "Scene/Components.h"
#include "GlobalBufferCache.h"
#include "Assets/MeshFormat.h"
#include "Rendering/GlobalPipeline.h"
#include "Rendering/UploadManager.h"
#include "FrameGraph/FrameGraph.h"

namespace
{
constexpr uint32_t EVICTION_AGE_THRESHOLD = 16;
constexpr uint32_t MAX_GROUPS_PER_FRAME = 256;
constexpr uint32_t STALE_REQUEST_FRAMES = 12;

struct StreamRequestsBufferLayout
{
	uint32_t load_count;
	uint32_t unload_count;
	uint32_t load_indices[MAX_STREAMING_REQUESTS];
	uint32_t unload_indices[MAX_UNLOAD_REQUESTS];
};
constexpr uint32_t STREAM_REQUESTS_BUFFER_SIZE = sizeof(StreamRequestsBufferLayout);

RHIBufferRef create_storage_buffer(uint32_t size, BufferUsage usage, bool use_staging, const char *debug_name)
{
	BufferDescription desc;
	desc.size = size;
	desc.usage = usage;
	desc.use_staging_buffer = use_staging;
	desc.storage_stride = sizeof(uint32_t);
	RHIBufferRef buffer = gDynamicRHI->createBuffer(desc);
	buffer->setDebugName(debug_name);
	return buffer;
}

// Writes [Meshlet headers | Vertices | Triangles] into dst.
void fill_group_data(uint8_t *dst, Engine::Mesh *mesh, uint32_t local_group_id, const Engine::MeshletFileView &file_view)
{
	Engine::MeshletGeometry::LODGroupDataInfo &info = mesh->meshlet_data->meshlet_lod_group_data_info[local_group_id];
	LODGroup &group = mesh->meshlet_data->meshlet_lod_groups[local_group_id];

	uint32_t meshlet_count = group.meshlet_count;
	uint32_t header_bytes = meshlet_count * sizeof(Meshlet);
	uint32_t disk_stride = MeshFormat::diskVertexStride(mesh->attribute_flags);
	uint32_t vertex_section_bytes = info.cpu_vertex_count * disk_stride;

	for (uint32_t m = 0; m < meshlet_count; m++)
	{
		Meshlet meshlet = mesh->meshlet_data->meshlets[group.first_meshlet + m];
		meshlet.vertex_offset = header_bytes + meshlet.vertex_offset * disk_stride;
		meshlet.triangle_offset = header_bytes + vertex_section_bytes + meshlet.triangle_offset * sizeof(uint32_t);
		memcpy(dst + m * sizeof(Meshlet), &meshlet, sizeof(Meshlet));
	}

	const uint8_t *src_verts = file_view.vertices_ptr + info.cpu_vertex_offset * disk_stride;
	memcpy(dst + header_bytes, src_verts, vertex_section_bytes);

	uint32_t *dst_tris = (uint32_t *)(dst + header_bytes + vertex_section_bytes);
	const uint8_t *src_tris = file_view.triangles_ptr + info.cpu_triangle_offset;
	for (uint32_t k = 0; k < info.cpu_triangle_count; k++)
		dst_tris[k] = src_tris[k];
}

uint32_t group_data_size(Engine::Mesh *mesh, uint32_t local_group_id)
{
	const Engine::MeshletGeometry::LODGroupDataInfo &info = mesh->meshlet_data->meshlet_lod_group_data_info[local_group_id];
	const LODGroup &group = mesh->meshlet_data->meshlet_lod_groups[local_group_id];
	return group.meshlet_count * sizeof(Meshlet)
		+ info.cpu_vertex_count * MeshFormat::diskVertexStride(mesh->attribute_flags)
		+ info.cpu_triangle_count * sizeof(uint32_t);
}
}

uint32_t GeometryStreaming::upload_group_data(Engine::Mesh *mesh, uint32_t local_group_id, const Engine::MeshletFileView &file_view, RHICommandList *cmd_list)
{
	PROFILE_CPU_FUNCTION();
	uint32_t data_size = group_data_size(mesh, local_group_id);

	UploadManager::StagedRange range = gUploadManager->stage(data_size);
	if (!range.data)
		return UINT32_MAX;

	fill_group_data(range.data, mesh, local_group_id, file_view);

	uint64_t offset = GlobalBufferCache::addMeshletGeometryData(range.buffer, range.offset, data_size, cmd_list);
	if (offset != UINT64_MAX)
		stats.addLoad(data_size);
	return offset;
}

void GeometryStreaming::init()
{
	stream_requests_gpu = create_storage_buffer(STREAM_REQUESTS_BUFFER_SIZE, BufferUsage::SHADER_WRITE_BUFFER, true, "Streaming Requests GPU");
	group_residency_gpu = create_storage_buffer(sizeof(GroupResidency), BufferUsage::SHADER_WRITE_BUFFER, true, "Meshlet Group Residency Buffer");
	group_ages_gpu = create_storage_buffer(sizeof(uint32_t), BufferUsage::SHADER_WRITE_BUFFER, true, "Meshlet Group Ages Buffer");

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		stream_requests_readback[i] = create_storage_buffer(STREAM_REQUESTS_BUFFER_SIZE, BufferUsage::READBACK_BUFFER, false, "Streaming Requests Readback");
}

// Free list range allocation
uint32_t GeometryStreaming::allocate_residency_range(uint32_t count)
{
	for (size_t i = 0; i < free_ranges.size(); i++)
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

	uint32_t offset = group_residency.size();
	group_residency.resize(offset + count);
	flat_to_mesh.resize(offset + count, nullptr);
	return offset;
}

void GeometryStreaming::registerMesh(Engine::Mesh *mesh, const Engine::MeshletFileView &file_view)
{
	if (!mesh->useMeshlets())
		return;
	if (registered_meshes.contains(mesh))
		return;

	const LODLevel &coarsest_lod = mesh->meshlet_data->meshlet_lod_levels.back();
	uint32_t groups_count = mesh->meshlet_data->meshlet_lod_groups.size();
	uint32_t base = allocate_residency_range(groups_count);

	RegisteredMesh &reg = registered_meshes[mesh];
	reg.file_view = file_view;
	reg.group_residency_offset = base;

	for (uint32_t i = 0; i < groups_count; i++)
	{
		flat_to_mesh[base + i] = mesh;

		GroupResidency &group = group_residency[base + i];
		if (i >= coarsest_lod.group_offset)
		{
			uint32_t data_size = group_data_size(mesh, i);
			eastl::vector<uint8_t> scratch(data_size);
			fill_group_data(scratch.data(), mesh, i, reg.file_view);

			RHIBufferRef staging = create_storage_buffer(data_size, BufferUsage::STAGING_BUFFER, false, "Group Upload Staging");
			staging->fill(scratch.data());

			group.geometry_buffer_offset = GlobalBufferCache::addMeshletGeometryData(staging, 0, data_size, gDynamicRHI->getCmdList());
			if (group.geometry_buffer_offset != UINT64_MAX)
				stats.resident_groups++;
		} else
		{
			group.geometry_buffer_offset = GROUP_NON_RESIDENT_ADDRESS_START;
		}
	}

	is_residency_dirty = true;
	stats.registered_mesh_count = (uint32_t)registered_meshes.size();
	stats.total_groups = (uint32_t)group_residency.size();
}

void GeometryStreaming::unregisterMesh(Engine::Mesh *mesh)
{
	auto it = registered_meshes.find(mesh);
	if (it == registered_meshes.end())
		return;

	uint32_t base = it->second.group_residency_offset;
	uint32_t groups_count = mesh->meshlet_data->meshlet_lod_groups.size();

	for (uint32_t i = 0; i < groups_count; i++)
	{
		uint32_t flat_index = base + i;
		uint64_t offset = group_residency[flat_index].geometry_buffer_offset;
		if (offset < GROUP_NON_RESIDENT_ADDRESS_START)
		{
			uint64_t data_size = group_data_size(mesh, i);
			pending_frees.push_back({offset, data_size, gDynamicRHI->getFrame()});
			stats.addUnload(data_size);
		}
		group_residency[flat_index].geometry_buffer_offset = GROUP_NON_RESIDENT_ADDRESS_START;
		flat_to_mesh[flat_index] = nullptr;
		load_queued.erase(flat_index);
	}

	free_ranges.push_back({base, groups_count});
	registered_meshes.erase(it);
	is_residency_dirty = true;
	stats.registered_mesh_count = (uint32_t)registered_meshes.size();
}

void GeometryStreaming::update()
{
	PROFILE_CPU_FUNCTION();
	stats.loads_last_frame = 0;
	stats.unloads_last_frame = 0;
	stats.bytes_loaded_last_frame = 0;
	stats.bytes_unloaded_last_frame = 0;

	if (gDynamicRHI->getFrame() >= MAX_FRAMES_IN_FLIGHT)
		process_gpu_requests(gDynamicRHI->getFrameInFlight());

	process_deferred_frees();

	eastl::vector<uint32_t> newly_loaded = upload_pending_groups(gDynamicRHI->getCmdList());

	stats.pending_load_queue_size = load_queued.size();
	stats.pending_frees_count = pending_frees.size();

	uint32_t ages_size = group_residency.size() * sizeof(uint32_t);
	bool ages_buffer_grew = false;
	if (group_ages_gpu->getSize() < ages_size)
	{
		group_ages_gpu = create_storage_buffer(ages_size, BufferUsage::SHADER_WRITE_BUFFER, true, "Meshlet Group Ages Buffer");
		ages_buffer_grew = true;
	}

	if (is_residency_dirty)
	{
		is_residency_dirty = false;
		uint32_t residency_size = group_residency.size() * sizeof(GroupResidency);
		if (group_residency_gpu->getSize() < residency_size)
			group_residency_gpu = create_storage_buffer(residency_size, BufferUsage::SHADER_WRITE_BUFFER, true, "Meshlet Group Residency Buffer");
		gUploadManager->queueUpload(GFXRID(GroupResidencyBuffer), 0, group_residency.data(), residency_size);
	}

	if (ages_buffer_grew)
	{
		gUploadManager->queueUpload(GFXRID(GroupAgesBuffer), 0, ages_size, [this, ages_size](uint8_t *dst)
		{
			uint32_t *ages = (uint32_t *)dst;
			memset(ages, 0, ages_size);
			for (auto &[mesh, reg] : registered_meshes)
			{
				const LODLevel &coarsest_lod = mesh->meshlet_data->meshlet_lod_levels.back();
				for (uint32_t i = 0; i < coarsest_lod.group_count; i++)
					ages[reg.group_residency_offset + coarsest_lod.group_offset + i] = PINNED_GROUP_AGE;
			}
		});
	}

	if (!newly_loaded.empty())
		gUploadManager->queueScatterFill(GFXRID(GroupAgesBuffer), eastl::span<const uint32_t>(newly_loaded.data(), newly_loaded.size()), nullptr, sizeof(uint32_t));

	uint32_t zeros[2] = {0, 0};
	gUploadManager->queueUpload(GFXRID(StreamRequestsBuffer), 0, zeros, sizeof(zeros));
}

void GeometryStreaming::importBuffers(FrameGraph &frame_graph)
{
	frame_graph.importBuffer(GFXRID(GroupResidencyBuffer), group_residency_gpu);
	frame_graph.importBuffer(GFXRID(StreamRequestsBuffer), stream_requests_gpu);
	frame_graph.importBuffer(GFXRID(GroupAgesBuffer), group_ages_gpu);
}

void GeometryStreaming::addAgeFilterAndReadbackPasses(FrameGraph &frame_graph)
{
	if (!group_residency.empty())
	{
		uint32_t group_count = group_residency.size();

		frame_graph.addCallbackPass("Streaming Age Filter",
		[&](RenderPassBuilder &builder)
		{
			builder.writeBuffer(GFXRID(GroupResidencyBuffer));
			builder.writeBuffer(GFXRID(GroupAgesBuffer));
			builder.writeBuffer(GFXRID(StreamRequestsBuffer));
		},
		[this, group_count](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			struct
			{
				uint32_t group_count;
				uint32_t age_threshold;
				uint32_t group_residency_buffer_id;
				uint32_t stream_requests_buffer_id;
				uint32_t group_ages_buffer_id;
			} constants;
			constants.group_count = group_count;
			constants.age_threshold = EVICTION_AGE_THRESHOLD;
			constants.group_residency_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupResidencyBuffer));
			constants.stream_requests_buffer_id = resources.getReadWriteBuffer(GFXRID(StreamRequestsBuffer));
			constants.group_ages_buffer_id = resources.getReadWriteBuffer(GFXRID(GroupAgesBuffer));

			gGlobalPipeline->setupComputePipeline(gDynamicRHI->createShader(L"shaders/gpu_driven/meshlet_stream_aging.hlsl", COMPUTE_SHADER));
			gGlobalPipeline->flushAndBind(cmd_list);
			gDynamicRHI->setConstantBufferData(0, &constants, sizeof(constants));
			cmd_list->dispatch((group_count + 63) / 64, 1, 1);
		});
	}

	frame_graph.addCallbackPass("Streaming Readback Copy",
	[](RenderPassBuilder &builder) { builder.setSideEffect(true); },
	[this](const RenderPassResources &, RHICommandList *cmd_list)
	{
		cmd_list->copyBuffer(stream_requests_gpu, stream_requests_readback[gDynamicRHI->getFrameInFlight()], 0, 0, STREAM_REQUESTS_BUFFER_SIZE);
	});
}

void GeometryStreaming::process_gpu_requests(int frame)
{
	PROFILE_CPU_FUNCTION();
	void *mapped;
	stream_requests_readback[frame]->map(&mapped);
	const StreamRequestsBufferLayout *requests = (const StreamRequestsBufferLayout *)mapped;

	uint32_t load_count = eastl::min(requests->load_count, MAX_STREAMING_REQUESTS);
	for (uint32_t i = 0; i < load_count; i++)
	{
		uint32_t flat_index = requests->load_indices[i];
		if (flat_index >= flat_to_mesh.size())
			continue;
		if (group_residency[flat_index].geometry_buffer_offset < GROUP_NON_RESIDENT_ADDRESS_START)
			continue;
		load_queued[flat_index] = gDynamicRHI->getFrame();
	}

	uint32_t unload_count = eastl::min(requests->unload_count, MAX_UNLOAD_REQUESTS);
	for (uint32_t i = 0; i < unload_count; i++)
	{
		uint32_t flat_index = requests->unload_indices[i];
		if (flat_index >= flat_to_mesh.size())
			continue;
		if (group_residency[flat_index].geometry_buffer_offset >= GROUP_NON_RESIDENT_ADDRESS_START)
			continue;

		Engine::Mesh *mesh = flat_to_mesh[flat_index];
		if (!mesh)
			continue;
		uint32_t local_group_id = flat_index - registered_meshes[mesh].group_residency_offset;
		const LODLevel &coarsest_lod = mesh->meshlet_data->meshlet_lod_levels.back();
		if (local_group_id >= coarsest_lod.group_offset)
			continue;

		is_residency_dirty = true;
		uint64_t freed_offset = group_residency[flat_index].geometry_buffer_offset;
		uint64_t freed_size = group_data_size(mesh, local_group_id);
		group_residency[flat_index].geometry_buffer_offset = GROUP_NON_RESIDENT_ADDRESS_START;
		pending_frees.push_back({freed_offset, freed_size, gDynamicRHI->getFrame()});
		stats.addUnload(freed_size);
	}

	stream_requests_readback[frame]->unmap();
}

void GeometryStreaming::process_deferred_frees()
{
	PROFILE_CPU_FUNCTION();
	auto expired = eastl::remove_if(pending_frees.begin(), pending_frees.end(), [&](const PendingFree &free)
	{
		if (gDynamicRHI->getFrame() - free.frame > MAX_FRAMES_IN_FLIGHT)
		{
			GlobalBufferCache::removeMeshletGeometryData(free.offset, free.size);
			return true;
		}
		return false;
	});
	pending_frees.erase(expired, pending_frees.end());
}

eastl::vector<uint32_t> GeometryStreaming::upload_pending_groups(RHICommandList *cmd_list)
{
	PROFILE_CPU_FUNCTION();
	eastl::vector<uint32_t> loaded;
	uint32_t loaded_groups = 0;

	for (auto it = load_queued.begin(); it != load_queued.end(); )
	{
		uint32_t flat_index = it->first;
		if (gDynamicRHI->getFrame() - it->second > STALE_REQUEST_FRAMES ||
			group_residency[flat_index].geometry_buffer_offset < GROUP_NON_RESIDENT_ADDRESS_START)
		{
			it = load_queued.erase(it);
			continue;
		}
		if (loaded_groups >= MAX_GROUPS_PER_FRAME)
		{
			++it;
			continue;
		}

		Engine::Mesh *mesh = flat_to_mesh[flat_index];
		if (!mesh)
		{
			it = load_queued.erase(it);
			continue;
		}
		RegisteredMesh &reg = registered_meshes[mesh];
		uint32_t local_group_id = flat_index - reg.group_residency_offset;
		uint32_t offset = upload_group_data(mesh, local_group_id, reg.file_view, cmd_list);
		if (offset == UINT32_MAX)
			break;

		group_residency[flat_index].geometry_buffer_offset = offset;
		loaded.push_back(flat_index);
		is_residency_dirty = true;
		loaded_groups++;
		it = load_queued.erase(it);
	}

	return loaded;
}