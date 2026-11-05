#pragma once
#include "Utils/Math.h"

class LinearAllocator
{
public:
	LinearAllocator(size_t capacity)
	{
		memory = (uint8_t *)malloc(capacity);
		current_ptr = memory;
		end_ptr = memory + capacity;
	}

	~LinearAllocator()
	{
		::free(memory);
	}

	void *allocate(size_t size, size_t alignment = 16)
	{
		uint8_t *aligned_address = current_ptr;
		if (alignment != 0)
			aligned_address = (uint8_t *)Math::alignedSize((size_t)current_ptr, alignment);

		if (aligned_address + size >= end_ptr)
			return nullptr;

		current_ptr = aligned_address + size;
		return aligned_address;
	}

	void free(void *pointer)
	{
		current_ptr = (uint8_t *)pointer;
	}

	void reset()
	{
		current_ptr = memory;
	}

	bool contains(const void *pointer) const
	{
		return pointer >= memory && pointer < end_ptr;
	}

private:
	uint8_t *memory;
	uint8_t *current_ptr;
	uint8_t *end_ptr;
};
