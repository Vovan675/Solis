#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "VulkanResources.h"

class VulkanBufferView;
class VulkanBuffer final: public RHIBuffer
{
public:
	VulkanBuffer(BufferDescription description);
	~VulkanBuffer();

	void fill(const void *sourceData) override;
	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override;

	RHIBufferView *getShaderResourceView() override;
	RHIBufferView *getUnorderedAccessView() override;

public:
	VkBuffer getBuffer() const { return buffer->resource; }
	VmaAllocation getAllocation() const { return allocation->resource; }

private:
	std::unique_ptr<VkBufferResource> buffer;
	std::unique_ptr<VkAllocationResource> allocation;

	Ref<VulkanBufferView> shader_resource_view;
	Ref<VulkanBufferView> unordered_access_view;

	bool is_mapped = false;
};

class VulkanBufferView final: public RHIBufferView
{
public:
	VulkanBufferView(BufferViewDescription description);
	~VulkanBufferView();
};