#pragma once
#include "RHI/RHICommandQueue.h"

class VulkanCommandQueue final: public RHICommandQueue
{
public:
	VulkanCommandQueue(VkQueue queue, bool create_fence = true);
	~VulkanCommandQueue();

	void execute(eastl::span<RHICommandList *> cmd_lists, VkSubmitInfo submit_info);

	void execute(eastl::span<RHICommandList *> cmd_lists) override;

	void signal(uint64_t fence_value) override
	{
		// Set fence value on execution end
		last_fence_value = fence_value;
	}
	
	void wait(uint64_t fence_value) override;

	void waitIdle() override;

	uint32_t getLastFenceValue() override
	{
		return last_fence_value;
	}

	VkQueue queue;
	VkFence fence;

	uint32_t last_fence_value = 0;
};