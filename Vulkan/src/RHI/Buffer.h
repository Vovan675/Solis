#pragma once
#include "vma/vk_mem_alloc.h"
#include "Log.h"
#include "Device.h"
#include "GPUResourceManager.h"

struct BufferDescription
{
	uint64_t size;
	bool useStagingBuffer;
	VkBufferUsageFlags bufferUsageFlags;
	uint64_t alignment = 0;
};

class Buffer : public GPUResource
{
public:
	VkBuffer bufferHandle;
	VmaAllocation allocation;
private:
	Buffer(BufferDescription description);
public:
	virtual ~Buffer();
	virtual void destroy();
	void fill(const void* sourceData);
	void fill(uint64_t offset, const void* sourceData, uint64_t size);

	// Used for persistent mapping
	void map(void** data);

	uint64_t getSize() const { return m_Description.size; }

	void setDebugName(const char *name);
	
	static std::shared_ptr<Buffer> create(BufferDescription description);
private:
	BufferDescription m_Description;
	bool is_mapped = false;
};

