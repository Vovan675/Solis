#pragma once
#include "RHI/DynamicRHI.h"
#include "RHI/RHIBuffer.h"
#include "VulkanResources.h"

class VulkanBuffer final: public RHIBuffer
{
public:
	VulkanBuffer(BufferDescription description);
	~VulkanBuffer();

	void map(void **data) override;
	void unmap() override;

	void setDebugName(const char *name) override;

	uint64_t getGPUAddress() const override;

	void transitState(RHICommandList *cmd_list, ResourceState new_state) override;

	bool isValid() const override { return buffer != nullptr; }

public:
	VkBuffer getBuffer() const { return buffer->resource; }
	VmaAllocation getAllocation() const { return allocation->resource; }

private:
	RHIBufferView *create_view(const BufferViewDescription &view_description) override;

	std::unique_ptr<VkBufferResource> buffer;
	std::unique_ptr<VkAllocationResource> allocation;

	ResourceState current_state = ResourceState::SHADER_RESOURCE;

	bool is_mapped = false;
};

class VulkanBufferView final: public RHIBufferView
{
public:
	VulkanBufferView(BufferViewDescription description);
	~VulkanBufferView();
};
