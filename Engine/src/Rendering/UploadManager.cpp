#include "pch.h"
#include <EASTL/hash_map.h>
#include "UploadManager.h"
#include "FrameGraph/FrameGraph.h"
#include "RHI/DynamicRHI.h"

UploadManager::Block UploadManager::create_block(uint32_t size)
{
	BufferDescription desc;
	desc.size = size;
	desc.memory = BufferMemory::UPLOAD;
	desc.stride = sizeof(uint32_t);

	Block block;
	block.buffer = gDynamicRHI->createBuffer(desc);
	block.buffer->setDebugName("UploadManager Staging Block");
	void *mapped;
	block.buffer->map(&mapped);
	block.mapped = (uint8_t *)mapped;
	return block;
}

void UploadManager::init(uint32_t per_frame_block_size)
{
	PROFILE_CPU_FUNCTION();
	block_size = per_frame_block_size;
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		frame_blocks[i].push_back(create_block(block_size));
}

void UploadManager::shutdown()
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		frame_blocks[i].clear();
	pending_copies.clear();
	pending_scatters.clear();
}

void UploadManager::beginFrame()
{
	current_frame = gDynamicRHI->getFrame() % MAX_FRAMES_IN_FLIGHT;
	frame_blocks[current_frame].resize(1);
	block_offset = 0;
	pending_copies.clear();
	pending_scatters.clear();
}

UploadManager::StagedRange UploadManager::stage(uint32_t size)
{
	eastl::vector<Block> &blocks = frame_blocks[current_frame];
	if (block_offset + size > blocks.back().buffer->getSize())
	{
		blocks.push_back(create_block(eastl::max(size, block_size)));
		block_offset = 0;
	}

	StagedRange range;
	range.data = blocks.back().mapped + block_offset;
	range.buffer = blocks.back().buffer;
	range.offset = block_offset;
	block_offset += size;
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

void UploadManager::queueUpload(GraphicsResourceName dst, uint32_t dst_offset, const void *data, uint32_t size)
{
	StagedRange range = stage(size);
	memcpy(range.data, data, size);
	push_copy(dst, dst_offset, range.buffer, range.offset, size);
}

void UploadManager::queueScatterFill(GraphicsResourceName dst, eastl::span<const uint32_t> indices,
	const void *element_data, uint32_t element_stride)
{
	if (indices.empty())
		return;

	StagedRange range = stage(element_stride);
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

	PROFILE_CPU_FUNCTION();
	eastl::hash_map<uint32_t, eastl::vector<CopyCommand>> copies_by_dst;
	for (const CopyCommand &cmd : pending_copies)
		copies_by_dst[cmd.dst.hashed_name].push_back(cmd);

	for (auto &[dst, cmds] : copies_by_dst)
	{
		GraphicsResourceName dst_name = cmds.front().dst;

		fg.addCallbackPass(eastl::string("UploadManager Copy: ") + dst_name.name,
		[&](RenderPassBuilder &builder)
		{
			builder.writeCopyBuffer(dst_name);
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
			builder.writeCopyBuffer(dst_name);
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
