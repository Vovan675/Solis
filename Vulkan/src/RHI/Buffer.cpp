#include "pch.h"
#include "Buffer.h"
#include "VkWrapper.h"
#include "Rendering/Renderer.h"
#include "VkUtils.h"

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

	VkWrapper::createBuffer(bufferSize, bufferUsageFlags, memory_usage, bufferHandle, allocation, description.alignment);
}

Buffer::~Buffer()
{
	// TODO: fix device hung on big scenes. Maybe per frame deletion queue? Check if issue exists
	if (allocation)
	{
		if (is_mapped)
			vmaUnmapMemory(VkWrapper::allocator, allocation);
		Renderer::deleteResource(RESOURCE_TYPE_MEMORY, allocation);
		allocation = nullptr;
	}

	if (bufferHandle)
		Renderer::deleteResource(RESOURCE_TYPE_BUFFER, bufferHandle);
	bufferHandle = nullptr;
}

void Buffer::fill(const void* sourceData)
{
	VkDeviceSize bufferSize = m_Description.size;

	if (m_Description.useStagingBuffer)
	{
		// Staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		VkWrapper::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation, m_Description.alignment);

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

void Buffer::fill(uint64_t offset, const void *sourceData, uint64_t size)
{
	// TODO
}

void Buffer::map(void **data)
{
	// Map buffer memory to CPU accessible memory
	vmaMapMemory(VkWrapper::allocator, allocation, data);
	is_mapped = true;
}

void Buffer::setDebugName(const char *name)
{
	VkUtils::setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)bufferHandle, name);
}
