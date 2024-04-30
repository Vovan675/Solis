#pragma once
#include "vulkan/vulkan.h"
#include "Log.h"

class CommandBuffer
{
public:
	CommandBuffer()
	{}

	void init(VkDevice device, VkCommandPool command_pool)
	{
		this->device = device;
		this->command_pool = command_pool;

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

	~CommandBuffer()
	{
		vkDestroyFence(device, in_flight_fence, nullptr);
	}

	void waitFence()
	{
		vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	}

	void open()
	{
		if (is_open)
			return;

		vkResetFences(device, 1, &in_flight_fence);

		VkCommandBufferBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkResetCommandBuffer(command_buffer, 0);
		vkBeginCommandBuffer(command_buffer, &info);

		is_open = true;
	}

	void close()
	{
		CHECK_ERROR(vkEndCommandBuffer(command_buffer));
		is_open = false;
	}

	VkCommandBuffer get_buffer() const
	{
		return command_buffer;
	}

	VkFence get_fence() const
	{
		return in_flight_fence;
	}

private:
	bool is_open = false;
	VkCommandBuffer command_buffer;
	VkFence in_flight_fence;

	VkDevice device;
	VkCommandPool command_pool;
};