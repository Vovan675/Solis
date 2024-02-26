#include "pch.h"
#include "Buffer.h"
#include "VkWrapper.h"

Buffer::Buffer( BufferDescription description)
	: m_Description(description)
{
	VkDeviceSize bufferSize = description.size;

	VkBufferUsageFlags bufferUsageFlags = description.bufferUsageFlags;
	VkMemoryPropertyFlags memoryFlags = 0;
	if (description.useStagingBuffer)
	{
		bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	else
	{
		memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

	VkWrapper::createBuffer(bufferSize, bufferUsageFlags, memoryFlags, bufferHandle, memoryHandle);
}

Buffer::~Buffer()
{
	vkDestroyBuffer(VkWrapper::device->logicalHandle, bufferHandle, nullptr);
	vkFreeMemory(VkWrapper::device->logicalHandle, memoryHandle, nullptr);
}

void Buffer::Fill(const void* sourceData)
{
	VkDeviceSize bufferSize = m_Description.size;

	if (m_Description.useStagingBuffer)
	{
		// Staging buffer
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		VkWrapper::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		// Map buffer memory to CPU accessible memory
		void* data;
		vkMapMemory(VkWrapper::device->logicalHandle, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, sourceData, bufferSize);
		vkUnmapMemory(VkWrapper::device->logicalHandle, stagingBufferMemory);

		// Copy from staging to buffer
		VkWrapper::copyBuffer(stagingBuffer, bufferHandle, bufferSize);

		// Destroy staging buffer
		vkDestroyBuffer(VkWrapper::device->logicalHandle, stagingBuffer, nullptr);
		vkFreeMemory(VkWrapper::device->logicalHandle, stagingBufferMemory, nullptr);
	}
	else
	{
		// Map buffer memory to CPU accessible memory
		void* data;
		vkMapMemory(VkWrapper::device->logicalHandle, memoryHandle, 0, bufferSize, 0, &data);
		memcpy(data, sourceData, bufferSize);
		vkUnmapMemory(VkWrapper::device->logicalHandle, memoryHandle);
	}
}

void Buffer::Map(void **data)
{
	// Map buffer memory to CPU accessible memory
	vkMapMemory(VkWrapper::device->logicalHandle, memoryHandle, 0, m_Description.size, 0, data);
}
