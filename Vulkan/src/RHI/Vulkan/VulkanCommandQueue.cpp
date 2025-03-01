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

void VulkanCommandQueue::execute(RHICommandList *cmd_list, VkSubmitInfo submit_info)
{
	VulkanCommandList *native_cmd_list = static_cast<VulkanCommandList *>(cmd_list);

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &native_cmd_list->cmd_buffer;

	// Reset fence before submission.
	vkResetFences(VulkanUtils::getNativeRHI()->device->logicalHandle, 1, &fence);
	vkQueueSubmit(queue, 1, &submit_info, fence);
}

void VulkanCommandQueue::execute(RHICommandList *cmd_list)
{
	VulkanCommandList *native_cmd_list = static_cast<VulkanCommandList *>(cmd_list);

	VkSubmitInfo submitInfo = {};
	execute(cmd_list, submitInfo);
}

void VulkanCommandQueue::wait(uint64_t fence_value)
{
	// Wait for fence to complete
	vkWaitForFences(VulkanUtils::getNativeRHI()->device->logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
}
