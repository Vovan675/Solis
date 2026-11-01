#pragma once
#include "RHI/RHICommandQueue.h"

class VulkanCommandQueue final: public RHICommandQueue
{
public:
	VulkanCommandQueue(VkQueue queue);
	~VulkanCommandQueue();

	uint64_t execute(eastl::span<RHICommandList *> cmd_lists, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore);
	uint64_t execute(eastl::span<RHICommandList *> cmd_lists) override;
	void waitForFence(uint64_t fence_value) override;
	void waitIdle() override;

	VkQueue queue;
	VkSemaphore fence;
};
