#include "pch.h"
#include "vma/vk_mem_alloc.h"
#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanBuffer::VulkanBuffer(BufferDescription description) : RHIBuffer(description)
{
	VkDeviceSize bufferSize = description.size;

	VkBufferUsageFlags bufferUsageFlags = 0;

	if (description.usage & VERTEX_BUFFER)
		bufferUsageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (description.usage & INDEX_BUFFER)
		bufferUsageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (description.usage & UNIFORM_BUFFER)
		bufferUsageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

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

	VulkanUtils::createBuffer(bufferSize, bufferUsageFlags, memory_usage, buffer_handle, allocation, description.alignment);
}

VulkanBuffer::~VulkanBuffer()
{
	destroy();
}

void VulkanBuffer::destroy()
{
	auto *native_rhi = VulkanUtils::getNativeRHI();
	if (is_mapped)
		unmap();
	if (buffer_handle)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_BUFFER, buffer_handle});
		buffer_handle = nullptr;
	}

	if (allocation)
	{
		native_rhi->gpu_release_queue[native_rhi->current_frame].push_back({RESOURCE_TYPE_MEMORY, allocation});
		allocation = nullptr;
	}

}

void VulkanBuffer::fill(const void *sourceData)
{
	if (!sourceData)
		return;
	PROFILE_CPU_FUNCTION();

	auto native_rhi = VulkanUtils::getNativeRHI();
	uint64_t buffer_size = description.size;

	if (description.useStagingBuffer)
	{
		// Staging buffer
		VkBuffer stagingBuffer;
		VmaAllocation stagingAllocation;
		VulkanUtils::createBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY, stagingBuffer, stagingAllocation, description.alignment);

		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(native_rhi->allocator, stagingAllocation, &data);
		memcpy(data, sourceData, buffer_size);
		vmaUnmapMemory(native_rhi->allocator, stagingAllocation);

		// Copy from staging to buffer
		VulkanUtils::copyBuffer(stagingBuffer, buffer_handle, buffer_size);

		// Destroy staging buffer
		vkDestroyBuffer(native_rhi->device->logicalHandle, stagingBuffer, nullptr);
		vmaFreeMemory(native_rhi->allocator, stagingAllocation);
	}
	else
	{
		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(native_rhi->allocator, allocation, &data);
		memcpy(data, sourceData, buffer_size);
		vmaUnmapMemory(native_rhi->allocator, allocation);
	}
}

void VulkanBuffer::map(void **data)
{
	if (is_mapped)
		return;
	// Map buffer memory to CPU accessible memory
	vmaMapMemory(VulkanUtils::getNativeRHI()->allocator, allocation, data);
	is_mapped = true;
}

void VulkanBuffer::unmap()
{
	if (!is_mapped)
		return;
	vmaUnmapMemory(VulkanUtils::getNativeRHI()->allocator, allocation);
	is_mapped = false;
}

void VulkanBuffer::setDebugName(const char *name)
{
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer_handle, name);
}