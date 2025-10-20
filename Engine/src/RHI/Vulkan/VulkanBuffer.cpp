#include "pch.h"
#include "vma/vk_mem_alloc.h"
#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanBuffer::VulkanBuffer(BufferDescription description) : RHIBuffer(description)
{
	VkDeviceSize bufferSize = description.size;

	VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	if (description.usage & VERTEX_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (description.usage & INDEX_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (description.usage & UNIFORM_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (description.usage & (UAV_BUFFER | STORAGE_BUFFER))
		usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (description.usage & ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
	if (description.usage & ACCELERATION_STRUCTURE_STORAGE_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	if (description.usage & SHADER_BINGING_TABLE_BUFFER)
		usage_flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

	VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO;
	if (description.useStagingBuffer)
		memory_usage = VMA_MEMORY_USAGE_GPU_ONLY;
	else
		memory_usage = VMA_MEMORY_USAGE_CPU_ONLY;

	buffer = std::make_unique<VkBufferResource>();
	allocation = std::make_unique<VkAllocationResource>();
	VulkanUtils::createBuffer(bufferSize, usage_flags, memory_usage, buffer->resource, allocation->resource, description.alignment);
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

	native_rhi->releaseGPUResource(buffer.release());
	native_rhi->releaseGPUResource(allocation.release());
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
		BufferDescription staging_buffer_description;
		staging_buffer_description.size = description.size;
		staging_buffer_description.alignment = description.alignment;
		staging_buffer_description.useStagingBuffer = false;
		RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_description);

		// Map buffer memory to CPU accessible memory
		void* data;
		staging_buffer->map(&data);
		memcpy(data, sourceData, buffer_size);
		staging_buffer->unmap();

		// Copy from staging to buffer
		RHICommandList *copy_cmd_list = gDynamicRHI->getCmdListCopy();
		copy_cmd_list->open();
		copy_cmd_list->copyBuffer(staging_buffer, this, 0, 0, buffer_size);
		copy_cmd_list->close();
		gDynamicRHI->getCmdQueueCopy()->execute(copy_cmd_list);
		gDynamicRHI->getCmdQueueCopy()->waitIdle();
	}
	else
	{
		// Map buffer memory to CPU accessible memory
		void* data;
		vmaMapMemory(native_rhi->allocator, allocation->resource, &data);
		memcpy(data, sourceData, buffer_size);
		vmaUnmapMemory(native_rhi->allocator, allocation->resource);
	}
}

void VulkanBuffer::map(void **data)
{
	if (is_mapped)
		return;
	// Map buffer memory to CPU accessible memory
	vmaMapMemory(VulkanUtils::getNativeRHI()->allocator, allocation->resource, data);
	is_mapped = true;
}

void VulkanBuffer::unmap()
{
	if (!is_mapped)
		return;
	vmaUnmapMemory(VulkanUtils::getNativeRHI()->allocator, allocation->resource);
	is_mapped = false;
}

void VulkanBuffer::setDebugName(const char *name)
{
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_BUFFER, (uint64_t)buffer->resource, name);
}

uint64_t VulkanBuffer::getGPUAddress() const
{
	VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
	bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	bufferDeviceAI.buffer = buffer->resource;
	return VulkanUtils::vkGetBufferDeviceAddressKHR(&bufferDeviceAI);
}
