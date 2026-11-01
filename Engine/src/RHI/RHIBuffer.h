#pragma once
#include "RHIDefinitions.h"

class RHICommandList;

class RHIBufferView : public RefCounted
{
public:
	RHIBufferView(BufferViewDescription description): description(description) {}
	virtual ~RHIBufferView() = default;

	const BufferViewDescription &getDescription() const { return description; }
	uint32_t getBindlessIndex() const { return bindless_index; }

protected:
	BufferViewDescription description;
	uint32_t bindless_index = 0;
};

class RHIBuffer : public RefCounted
{
public:
	RHIBuffer(BufferDescription description): description(description) {}
	virtual ~RHIBuffer() = default;

	void fill(const void *source_data);
	virtual void map(void **data) = 0;
	virtual void unmap() = 0;

	const BufferDescription &getDescription() const { return description; }
	uint64_t getSize() const { return description.size; }
	BufferUsage getUsage() const { return description.usage; }
	uint32_t getStride() const { return description.storage_stride; }
	virtual void setDebugName(const char *name) = 0;
	virtual uint64_t getGPUAddress() const = 0;

	virtual void transitState(RHICommandList *cmd_list, ResourceState new_state) = 0;

	virtual bool isValid() const { return true; }

	RHIBufferView *getShaderResourceView();
	RHIBufferView *getUnorderedAccessView();

protected:
	virtual RHIBufferView *create_view(const BufferViewDescription &view_description) = 0;

	BufferDescription description;

	Ref<RHIBufferView> shader_resource_view;
	Ref<RHIBufferView> unordered_access_view;
};
