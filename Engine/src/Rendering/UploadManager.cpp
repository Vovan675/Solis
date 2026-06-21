#include "pch.h"
#include <EASTL/hash_map.h>
#include "UploadManager.h"
#include "FrameGraph/FrameGraph.h"
#include "RHI/DynamicRHI.h"

void UploadManager::init(uint32_t per_frame_ring_size)
{
	ring_size = per_frame_ring_size;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = ring_size;
		desc.usage = BufferUsage::STAGING_BUFFER;
		desc.use_staging_buffer = false;
		desc.storage_stride = sizeof(uint32_t);
		ring_buffers[i] = gDynamicRHI->createBuffer(desc);
		ring_buffers[i]->setDebugName("UploadManager Ring Buffer");
		ring_buffers[i]->map(&ring_mapped[i]);
	}
}

void UploadManager::shutdown()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		ring_buffers[i] = nullptr;
		ring_mapped[i] = nullptr;
	}
	pending_copies.clear();
	pending_scatters.clear();
}

void UploadManager::beginFrame()
{
	current_frame = gDynamicRHI->getFrame() % MAX_FRAMES_IN_FLIGHT;
	ring_offset = 0;
	pending_copies.clear();
	pending_scatters.clear();
}

UploadManager::StagedRange UploadManager::stage(uint32_t size)
{
	if (ring_offset + size > ring_size)
	{
		CORE_WARN("UploadManager::stage(): not enough frame ring buffer size");
		return {};
	}

	StagedRange range;
	range.data = (uint8_t *)ring_mapped[current_frame] + ring_offset;
	range.buffer = ring_buffers[current_frame];
	range.offset = ring_offset;
	ring_offset += size;
	return range;
}

void UploadManager::push_copy(GraphicsResourceName dst, uint32_t dst_offset, RHIBuffer *src, uint32_t src_offset, uint32_t size)
{
	CopyCommand cmd;
	cmd.dst = dst;
	cmd.dst_offset = dst_offset;
	cmd.src = src;
	cmd.src_offset = src_offset;
	cmd.size = size;
	pending_copies.push_back(cmd);
}

bool UploadManager::queueUpload(GraphicsResourceName dst, uint32_t dst_offset, const void *data, uint32_t size)
{
	StagedRange range = stage(size);
	if (!range.data)
		return false;
	memcpy(range.data, data, size);
	push_copy(dst, dst_offset, range.buffer, range.offset, size);
	return true;
}

void UploadManager::queueScatterFill(GraphicsResourceName dst, eastl::span<const uint32_t> indices,
	const void *element_data, uint32_t element_stride)
{
	if (indices.empty())
		return;

	StagedRange range = stage(element_stride);
	if (!range.data)
		return;
	if (element_data)
		memcpy(range.data, element_data, element_stride);
	else
		memset(range.data, 0, element_stride);

	ScatterCommand cmd;
	cmd.dst = dst;
	cmd.src = range.buffer;
	cmd.src_offset = range.offset;
	cmd.element_stride = element_stride;
	cmd.indices.assign(indices.begin(), indices.end());
	pending_scatters.push_back(eastl::move(cmd));
}

void UploadManager::flush(FrameGraph &fg)
{
	if (pending_copies.empty() && pending_scatters.empty())
		return;

	eastl::hash_map<uint32_t, eastl::vector<CopyCommand>> copies_by_dst;
	for (const CopyCommand &cmd : pending_copies)
		copies_by_dst[cmd.dst.hashed_name].push_back(cmd);

	for (auto &[dst, cmds] : copies_by_dst)
	{
		GraphicsResourceName dst_name = cmds.front().dst;

		fg.addCallbackPass(eastl::string("UploadManager Copy: ") + dst_name.name,
		[&](RenderPassBuilder &builder)
		{
			builder.writeBuffer(dst_name);
		},
		[batch = eastl::move(cmds)](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			RHIBuffer *dst = resources.getBuffer(batch.front().dst);
			for (const CopyCommand &cmd : batch)
				cmd_list->copyBuffer(cmd.src, dst, cmd.src_offset, cmd.dst_offset, cmd.size);
		});
	}

	eastl::hash_map<uint32_t, eastl::vector<ScatterCommand>> scatters_by_dst;
	for (const ScatterCommand &cmd : pending_scatters)
		scatters_by_dst[cmd.dst.hashed_name].push_back(cmd);

	for (auto &[dst, cmds] : scatters_by_dst)
	{
		GraphicsResourceName dst_name = cmds.front().dst;

		fg.addCallbackPass(eastl::string("UploadManager Scatter Fill: ") + dst_name.name,
		[&](RenderPassBuilder &builder)
		{
			builder.writeBuffer(dst_name);
		},
		[batch = eastl::move(cmds)](const RenderPassResources &resources, RHICommandList *cmd_list)
		{
			RHIBuffer *dst = resources.getBuffer(batch.front().dst);
			for (const ScatterCommand &cmd : batch)
			{
				for (uint32_t index : cmd.indices)
					cmd_list->copyBuffer(cmd.src, dst, cmd.src_offset, index * cmd.element_stride, cmd.element_stride);
			}
		});
	}

	pending_copies.clear();
	pending_scatters.clear();
}
