#include "pch.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanCommandQueue::VulkanCommandQueue(VkQueue queue, bool create_fence): queue(queue)
{
	if (create_fence)
	{
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0; // unsignaled initially
		vkCreateFence(VulkanUtils::getNativeRHI()->device->logicalHandle, &fenceInfo, nullptr, &fence);
		VulkanUtils::setDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)fence, "Command Queue Fence");
	}
}

VulkanCommandQueue::~VulkanCommandQueue()
{
	if (fence)
	{
		vkDestroyFence(VulkanUtils::getNativeRHI()->device->logicalHandle, fence, nullptr);
		fence = nullptr;
	}
}

void VulkanCommandQueue::execute(eastl::span<RHICommandList *> cmd_lists, VkSubmitInfo submit_info)
{
	eastl::vector<VkCommandBuffer> cmd_buffers;
	cmd_buffers.reserve(cmd_lists.size());
	for (RHICommandList *cmd_list : cmd_lists)
		cmd_buffers.push_back(static_cast<VulkanCommandList *>(cmd_list)->cmd_buffer);

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = cmd_buffers.size();
	submit_info.pCommandBuffers = cmd_buffers.data();

	// Reset fence before submission.
	vkResetFences(VulkanUtils::getNativeRHI()->device->logicalHandle, 1, &fence);
	vkQueueSubmit(queue, 1, &submit_info, fence);
}

void VulkanCommandQueue::execute(eastl::span<RHICommandList *> cmd_lists)
{
	VkSubmitInfo submitInfo = {};
	execute(cmd_lists, submitInfo);
}

void VulkanCommandQueue::wait(uint64_t fence_value)
{
	PROFILE_CPU_FUNCTION();
	// Wait for fence to complete
	vkWaitForFences(VulkanUtils::getNativeRHI()->device->logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
}

void VulkanCommandQueue::waitIdle()
{
	PROFILE_CPU_FUNCTION();
	vkQueueWaitIdle(queue);
}
