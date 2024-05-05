#include "pch.h"
#include "Buffer.h"
#include "VkWrapper.h"

Buffer::Buffer(BufferDescription description)
	: m_Description(description)
{
	VkDeviceSize bufferSize = description.size;

	VkBufferUsageFlags bufferUsageFlags = description.bufferUsageFlags;
	VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO;
	if (description.useStagingBuffer)
	{
		bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
	}
	else
	{
		memory_usage = VMA_MEMORY_USAGE_CPU_ONLY;
	}

	VkWrapper::createBuffer(bufferSize, bufferUsageFlags, memory_usage, bufferHandle, allocation);
}

Buffer::~Buffer()
{
	vmaUnmapMemory(VkWrapper::allocator, allocation);
	vkDestroyBuffer(VkWrapper::device->logicalHandle, bufferHandle, nullptr);
	vmaFreeMemory(VkWrapper::allocator, allocation);
}

void Buffer::fill(const void* sourceData)
{
	VkDeviceSize bufferSize = m_Description.size;

	if (m_Description.useStagingBuffer)
	{
		// Staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		VkWrapper::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation);

		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(VkWrapper::allocator, stagingAllocation, &data);
		memcpy(data, sourceData, bufferSize);
		vmaUnmapMemory(VkWrapper::allocator, stagingAllocation);

		// Copy from staging to buffer
		VkWrapper::copyBuffer(stagingBuffer, bufferHandle, bufferSize);

		// Destroy staging buffer
		vkDestroyBuffer(VkWrapper::device->logicalHandle, stagingBuffer, nullptr);
		vmaFreeMemory(VkWrapper::allocator, stagingAllocation);
	}
	else
	{
		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(VkWrapper::allocator, allocation, &data);
		memcpy(data, sourceData, bufferSize);
		vmaUnmapMemory(VkWrapper::allocator, allocation);
	}
}

void Buffer::map(void **data)
{
	// Map buffer memory to CPU accessible memory
	vmaMapMemory(VkWrapper::allocator, allocation, data);
}
