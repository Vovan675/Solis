#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"

class VulkanBuffer : public RHIBuffer
{
public:
	VulkanBuffer(BufferDescription description);
	~VulkanBuffer();

	void destroy();

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	VkBuffer buffer_handle;
	VmaAllocation allocation;
	bool is_mapped = false;
};