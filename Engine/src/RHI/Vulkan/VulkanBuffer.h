#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "VulkanResources.h"

class VulkanBuffer final: public RHIBuffer
{
public:
	VulkanBuffer(BufferDescription description);
	~VulkanBuffer();

	void destroy();

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override;

	std::unique_ptr<VkBufferResource> buffer;
	std::unique_ptr<VkAllocationResource> allocation;
	bool is_mapped = false;
};