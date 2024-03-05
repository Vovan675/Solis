#include "pch.h"
#include "VulkanApp.h"
#include "imgui.h"

VulkanApp::VulkanApp()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //FUCK OFF OPENGL
	window = glfwCreateWindow(800, 600, "Hello Vulkan", nullptr, nullptr);
	glfwSwapInterval(0);

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height)
	{
		static_cast<VulkanApp *>(glfwGetWindowUserPointer(window))->framebuffer_resize_callback(window, width, height);
	});

	glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		static_cast<VulkanApp *>(glfwGetWindowUserPointer(window))->key_callback(window, key, scancode, action, mods);
	});

	Log::Init();
	VkWrapper::init(window);

	init_depth();
	init_sync_objects();
}

void VulkanApp::run()
{
	double prev_time = glfwGetTime();
	float delta_seconds = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Wait when queue for this frame is completed (device -> host sync)
		VkWrapper::command_buffers[currentFrame].waitFence();

		// Acquire image and trigger semaphore
		uint32_t image_index;
		VkResult result = vkAcquireNextImageKHR(VkWrapper::device->logicalHandle, VkWrapper::swapchain->swapchainHandle, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &image_index);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain();
			continue;
		} else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to acquire next image");
		}

		updateBuffers(delta_seconds, image_index);
		update(delta_seconds);

		// Record commands
		render(VkWrapper::command_buffers[currentFrame], image_index);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Wait when semaphore when image will get acquired
		VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		VkCommandBuffer command_buffer = VkWrapper::command_buffers[currentFrame].get_buffer();
		submitInfo.pCommandBuffers = &command_buffer;
		// Signal semaphore when queue submitted
		VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		// Also trigger fence when queue completed to work with this 'currentFrame'
		CHECK_ERROR(vkQueueSubmit(VkWrapper::device->graphicsQueue, 1, &submitInfo, VkWrapper::command_buffers[currentFrame].get_fence()));

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		// Wait until queue submitted before present it
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = {VkWrapper::swapchain->swapchainHandle};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &image_index;
		result = vkQueuePresentKHR(VkWrapper::device->presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized)
		{
			framebuffer_resized = false;
			recreate_swapchain();
		} else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to present swap chain image");
		}
		//vkQueueWaitIdle(VkWrapper::device->presentQueue);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		delta_seconds = prev_time - glfwGetTime();
		prev_time = glfwGetTime();
	}

	cleanup();
}

void VulkanApp::render(CommandBuffer &command_buffer, uint32_t image_index)
{
	command_buffer.open();

	{
		// Set swapchain color image layout for writing
		VkImageMemoryBarrier2 image_memory_barrier{};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_memory_barrier.srcAccessMask = 0;
		image_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_memory_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		image_memory_barrier.image = VkWrapper::swapchain->swapchainImages[image_index];
		image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = 1;

		VkDependencyInfo dependency_info{};
		dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &image_memory_barrier;

		vkCmdPipelineBarrier2(command_buffer.get_buffer(), &dependency_info);
	}


	// Begin dynamic rendering
	VkRenderingAttachmentInfo color_attachment_info{};
	color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	color_attachment_info.imageView = VkWrapper::swapchain->swapchainImageViews[image_index];
	color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment_info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

	VkRenderingAttachmentInfo depth_stencil_attachment_info{};
	depth_stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_stencil_attachment_info.imageView = depthStencilImages[image_index]->imageView;
	depth_stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_stencil_attachment_info.clearValue.depthStencil = {1.0f, 0};

	VkRenderingInfo rendering_info{};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea.offset = {0, 0};
	rendering_info.renderArea.extent = VkWrapper::swapchain->swapExtent;
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_attachment_info;
	rendering_info.pDepthAttachment = &depth_stencil_attachment_info;

	vkCmdBeginRendering(command_buffer.get_buffer(), &rendering_info);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)VkWrapper::swapchain->swapExtent.width;
	viewport.height = (float)VkWrapper::swapchain->swapExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer.get_buffer(), 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = VkWrapper::swapchain->swapExtent;
	vkCmdSetScissor(command_buffer.get_buffer(), 0, 1, &scissor);

	// Record commands
	recordCommands(command_buffer, image_index);

	vkCmdEndRendering(command_buffer.get_buffer());

	{
		// Set swapchain color image layout for presenting
		VkImageMemoryBarrier2 image_memory_barrier{};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_memory_barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		image_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		image_memory_barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
		image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		image_memory_barrier.image = VkWrapper::swapchain->swapchainImages[image_index];
		image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = 1;

		VkDependencyInfo dependency_info{};
		dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &image_memory_barrier;

		vkCmdPipelineBarrier2(command_buffer.get_buffer(), &dependency_info);
	}

	command_buffer.close();
}

void VulkanApp::cleanup()
{
	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
	VkWrapper::cleanup();
	depthStencilImages.clear();
	cleanupResources();
	cleanup_swapchain();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(VkWrapper::device->logicalHandle, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(VkWrapper::device->logicalHandle, renderFinishedSemaphores[i], nullptr);
	}
	VkWrapper::command_buffers.clear();

	VkWrapper::device = nullptr;
	vkDestroyInstance(VkWrapper::instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void VulkanApp::cleanup_swapchain()
{
	VkWrapper::swapchain = nullptr;
}

void VulkanApp::recreate_swapchain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);

	depthStencilImages.clear();

	VkWrapper::swapchain->create(width, height);
	init_depth();
}

void VulkanApp::init_depth()
{
	depthStencilImages.resize(VkWrapper::swapchain->swapchainImages.size());
	for (int i = 0; i < VkWrapper::swapchain->swapchainImages.size(); i++)
	{
		TextureDescription description;
		description.width = VkWrapper::swapchain->swapExtent.width;
		description.height = VkWrapper::swapchain->swapExtent.height;
		description.mipLevels = 1;
		description.numSamples = VK_SAMPLE_COUNT_1_BIT;
		description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		description.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthStencilImages[i] = std::make_shared<Texture>(description);
		depthStencilImages[i]->fill();
	}
}

void VulkanApp::init_sync_objects()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	//Create Semaphores info
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	//Create Fences info
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		CHECK_ERROR(vkCreateSemaphore(VkWrapper::device->logicalHandle, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
		CHECK_ERROR(vkCreateSemaphore(VkWrapper::device->logicalHandle, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
	}
}
