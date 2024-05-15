#include "pch.h"
#include "VulkanApp.h"
#include "imgui.h"
#include "Rendering/Renderer.h"

VulkanApp::VulkanApp()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //FUCK OFF OPENGL
	window = glfwCreateWindow(1440, 900, "Hello Vulkan", nullptr, nullptr);
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
		VkResult result = vkAcquireNextImageKHR(VkWrapper::device->logicalHandle, VkWrapper::swapchain->swapchain_handle, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &image_index);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain();
			continue;
		} else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to acquire next image");
		}

		last_fps_timer -= delta_seconds;
		if (last_fps_timer <= 0)
		{
			last_fps_timer = 0.3;
			last_fps = 1.0f / delta_seconds;
		}

		update(delta_seconds);
		updateBuffers(delta_seconds, image_index);

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
		VkSwapchainKHR swapChains[] = {VkWrapper::swapchain->swapchain_handle};
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

		Renderer::endFrame(image_index);
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		delta_seconds = glfwGetTime() - prev_time;
		prev_time = glfwGetTime();
	}

	cleanup();
}

void VulkanApp::render(CommandBuffer &command_buffer, uint32_t image_index)
{
	command_buffer.open();

	// Set swapchain color image layout for writing
	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
									VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									VkWrapper::swapchain->swapchain_images[image_index], VK_IMAGE_ASPECT_COLOR_BIT);

	// Record commands (they do what they want + output to swapchain_textures)
	recordCommands(command_buffer, image_index);

	// Set swapchain color image layout for presenting
	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
									VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
									VkWrapper::swapchain->swapchain_images[image_index], VK_IMAGE_ASPECT_COLOR_BIT);

	command_buffer.close();
}

void VulkanApp::cleanup()
{
	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
	VkWrapper::cleanup();
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

	VkWrapper::swapchain->create(width, height);
	Renderer::recreateScreenResources();
	onSwapchainRecreated(width, height);
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
