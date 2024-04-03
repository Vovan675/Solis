#pragma once
#include "vma/vk_mem_alloc.h"
#include "Log.h"
#include "Device.h"

struct BufferDescription
{
	uint64_t size;
	bool useStagingBuffer;
	VkBufferUsageFlags bufferUsageFlags;
};

class Buffer
{
public:
	VkBuffer bufferHandle;
	VmaAllocation allocation;
public:
	Buffer(BufferDescription description);
	virtual ~Buffer();
	void fill(const void* sourceData);

	// Used for persistent mapping
	void map(void** data);
private:
	BufferDescription m_Description;
};

