#include "pch.h"
#include "CommandBuffer.h"
#include "VkWrapper.h"

void CommandBuffer::init(bool one_time_submit)
{
	this->device = VkWrapper::device->logicalHandle;
	this->command_pool = VkWrapper::command_pool;
	this->one_time_submit = one_time_submit;

	// Create buffer
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	CHECK_ERROR(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));

	// Create fence
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	CHECK_ERROR(vkCreateFence(device, &fence_info, nullptr, &in_flight_fence));
}

void CommandBuffer::open()
{
	if (is_open)
		return;

	vkResetFences(device, 1, &in_flight_fence);

	VkCommandBufferBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	info.flags = one_time_submit ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
	vkResetCommandBuffer(command_buffer, 0);
	vkBeginCommandBuffer(command_buffer, &info);

	is_open = true;
}

void CommandBuffer::close()
{
	CHECK_ERROR(vkEndCommandBuffer(command_buffer));
	is_open = false;

	if (one_time_submit)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &command_buffer;
		vkQueueSubmit(VkWrapper::device->graphicsQueue, 1, &submitInfo, in_flight_fence);
	}
}
