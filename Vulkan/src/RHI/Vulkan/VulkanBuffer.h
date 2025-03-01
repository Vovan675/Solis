#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"

class VulkanBuffer : public RHIBuffer
{
public:
	VulkanBuffer(BufferDescription description);
	~VulkanBuffer();

	void destroy() override;

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	VkBuffer bufferHandle;
	VmaAllocation allocation;
	bool is_mapped = false;
};