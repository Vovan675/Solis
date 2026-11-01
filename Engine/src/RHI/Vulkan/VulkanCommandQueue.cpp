#include "pch.h"
#include "VulkanCommandQueue.h"
#include "VulkanCommandList.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

VulkanCommandQueue::VulkanCommandQueue(VkQueue queue): queue(queue)
{
	// Always use timeline semaphore (aka Fence in dx12)
	VkSemaphoreTypeCreateInfo timeline_semaphore_create_info{};
	timeline_semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timeline_semaphore_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_semaphore_create_info.initialValue = 0;

	VkSemaphoreCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	create_info.pNext = &timeline_semaphore_create_info;
	CHECK_ERROR(vkCreateSemaphore(VulkanUtils::getNativeRHI()->device->logicalHandle, &create_info, nullptr, &fence));
	VulkanUtils::setDebugName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)fence, "Command Queue Fence");
}

VulkanCommandQueue::~VulkanCommandQueue()
{
	vkDestroySemaphore(VulkanUtils::getNativeRHI()->device->logicalHandle, fence, nullptr);
}

uint64_t VulkanCommandQueue::execute(eastl::span<RHICommandList *> cmd_lists, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore)
{
	eastl::vector<VkCommandBufferSubmitInfo> cmd_buffer_submit_infos;
	cmd_buffer_submit_infos.reserve(cmd_lists.size());
	for (RHICommandList *cmd_list : cmd_lists)
	{
		VkCommandBufferSubmitInfo &info = cmd_buffer_submit_infos.emplace_back();
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		info.commandBuffer = static_cast<VulkanCommandList *>(cmd_list)->cmd_buffer;
	}

	// Wait when semaphore when image will get acquired
	VkSemaphoreSubmitInfo wait_info{};
	wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	wait_info.semaphore = wait_semaphore;
	wait_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	// Set fence value on execution end
	last_fence_value++;
	eastl::fixed_vector<VkSemaphoreSubmitInfo, 2> signal_infos;
	VkSemaphoreSubmitInfo &fence_info = signal_infos.emplace_back();
	fence_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	fence_info.semaphore = fence;
	fence_info.value = last_fence_value;
	fence_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	// Signal semaphore when queue submitted
	if (signal_semaphore != VK_NULL_HANDLE)
	{
		VkSemaphoreSubmitInfo &signal_info = signal_infos.emplace_back();
		signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		signal_info.semaphore = signal_semaphore;
		signal_info.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	}

	VkSubmitInfo2 submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit_info.waitSemaphoreInfoCount = wait_semaphore != VK_NULL_HANDLE ? 1 : 0;
	submit_info.pWaitSemaphoreInfos = &wait_info;
	submit_info.commandBufferInfoCount = cmd_buffer_submit_infos.size();
	submit_info.pCommandBufferInfos = cmd_buffer_submit_infos.data();
	submit_info.signalSemaphoreInfoCount = signal_infos.size();
	submit_info.pSignalSemaphoreInfos = signal_infos.data();
	vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE);
	return last_fence_value;
}

uint64_t VulkanCommandQueue::execute(eastl::span<RHICommandList *> cmd_lists)
{
	return execute(cmd_lists, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void VulkanCommandQueue::waitForFence(uint64_t fence_value)
{
	PROFILE_CPU_FUNCTION();
	// Wait for fence to complete
	VkSemaphoreWaitInfo wait_info{};
	wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = &fence;
	wait_info.pValues = &fence_value;
	vkWaitSemaphores(VulkanUtils::getNativeRHI()->device->logicalHandle, &wait_info, UINT64_MAX);
}

void VulkanCommandQueue::waitIdle()
{
	PROFILE_CPU_FUNCTION();
	vkQueueWaitIdle(queue);
}
