#pragma once
#include "RHIDefinitions.h"

class RHIBufferView;
class RHIBuffer : public RefCounted
{
public:
	RHIBuffer(BufferDescription description): description(description) {}
	virtual ~RHIBuffer() = default;

	virtual void fill(const void *sourceData) = 0;
	virtual void map(void **data) = 0;
	virtual void unmap() = 0;

	uint64_t getSize() const { return description.size; }
	BufferUsage getUsage() const { return description.usage; }
	uint32_t getStride() const { return description.storage_stride; }
	virtual void setDebugName(const char *name) = 0;
	virtual uint64_t getGPUAddress() const = 0;

	virtual RHIBufferView *getShaderResourceView() = 0;
	virtual RHIBufferView *getUnorderedAccessView() = 0;

protected:
	BufferDescription description;
};

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