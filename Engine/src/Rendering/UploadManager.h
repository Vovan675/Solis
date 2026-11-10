#pragma once
#include <EASTL/span.h>
#include <EASTL/vector.h>
#include "FrameGraph/FrameGraphRHIResources.h"
#include "RHI/RHIDefinitions.h"

class FrameGraph;

class UploadManager
{
public:
	struct StagedRange
	{
		uint8_t *data = nullptr;
		RHIBuffer *buffer = nullptr;
		uint32_t offset = 0;
	};

	void init(uint32_t per_frame_block_size = 32 * 1024 * 1024);
	void shutdown();

	void beginFrame();

	// Sub allocate from staging buffer
	StagedRange stage(uint32_t size);

	void queueUpload(GraphicsResourceName dst, uint32_t dst_offset, const void *data, uint32_t size);

	template<typename FillFunc>
	void queueUpload(GraphicsResourceName dst, uint32_t dst_offset, uint32_t size, FillFunc fill)
	{
		StagedRange range = stage(size);
		fill(range.data);
		push_copy(dst, dst_offset, range.buffer, range.offset, size);
	}

	// Copies a single element value into many indices. element_data = nullptr for zero fill.
	void queueScatterFill(GraphicsResourceName dst, eastl::span<const uint32_t> indices, const void *element_data, uint32_t element_stride);

	// TODO: queueScatter: write different data to different indices fast (usuing compute shader for this)

	void flush(FrameGraph &fg);

private:
	struct CopyCommand
	{
		GraphicsResourceName dst;
		uint32_t dst_offset;
		RHIBuffer *src;
		uint32_t src_offset;
		uint32_t size;
	};

	struct ScatterCommand
	{
		GraphicsResourceName dst;
		RHIBuffer *src;
		uint32_t src_offset;
		uint32_t element_stride;
		eastl::vector<uint32_t> indices;
	};

	struct Block
	{
		RHIBufferRef buffer;
		uint8_t *mapped = nullptr;
	};

	Block create_block(uint32_t size);
	void push_copy(GraphicsResourceName dst, uint32_t dst_offset, RHIBuffer *src, uint32_t src_offset, uint32_t size);

	// First block is reused, the rest are overflow
	eastl::vector<Block> frame_blocks[MAX_FRAMES_IN_FLIGHT];
	uint32_t block_size = 0;
	uint32_t block_offset = 0;
	int current_frame = 0;

	eastl::vector<CopyCommand> pending_copies;
	eastl::vector<ScatterCommand> pending_scatters;
};

extern UploadManager *gUploadManager;
