#pragma once
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
	VkDeviceMemory memoryHandle;
public:
	Buffer(BufferDescription description);
	virtual ~Buffer();
	void Fill(const void* sourceData);

	// Used for persistent mapping
	void Map(void** data);
private:
	BufferDescription m_Description;
};

