#pragma once
#include "RHIDefinitions.h"

class RHIBuffer : public RefCounted
{
public:
	RHIBuffer(BufferDescription description): description(description) {}

	virtual void fill(const void *sourceData) = 0;
	virtual void map(void **data) = 0;
	virtual void unmap() = 0;

	uint64_t getSize() const { return description.size; }
	virtual void setDebugName(const char *name) {}

	BufferDescription description;
};
