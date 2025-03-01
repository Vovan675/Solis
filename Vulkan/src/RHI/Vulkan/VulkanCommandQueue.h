#pragma once
#include "RHI/RHICommandQueue.h"

class VulkanCommandQueue: public RHICommandQueue
{
public:
	VulkanCommandQueue(VkQueue queue, bool create_fence = true);
	~VulkanCommandQueue();

	void execute(RHICommandList *cmd_list, VkSubmitInfo submit_info);

	void execute(RHICommandList *cmd_list) override;

	void signal(uint64_t fence_value) override
	{
		// Set fence value on execution end
		last_fence_value = fence_value;
	}
	
	void wait(uint64_t fence_value) override;

	void waitIdle() override
	{
		vkQueueWaitIdle(queue);
	}

	uint32_t getLastFenceValue() override
	{
		return last_fence_value;
	}

	VkQueue queue;
	VkFence fence;

	uint32_t last_fence_value = 0;
};