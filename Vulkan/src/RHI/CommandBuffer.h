#pragma once
#include "vulkan/vulkan.h"
#include "Log.h"

class CommandBuffer
{
public:
	CommandBuffer()
	{}
	
	~CommandBuffer()
	{
		vkDeviceWaitIdle(device);
		vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
		vkDestroyFence(device, in_flight_fence, nullptr);
	}

	void init(bool one_time_submit = false);

	void waitFence()
	{
		vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	}

	void open();
	void close();

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

	bool one_time_submit = false;
};