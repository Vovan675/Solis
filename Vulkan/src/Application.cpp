#include "pch.h"
#include "Application.h"
#include "Rendering/Renderer.h"
#include "Assets/AssetManager.h"
#include "Scene/Scene.h"
#include "GLFW/glfw3native.h"
#include "RHI/GPUResourceManager.h"
#include "FrameGraph/TransientResources.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

Input input;
GPUResourceManager gpu_resource_manager;

Application::Application()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //FUCK OFF OPENG
	window = glfwCreateWindow(1920, 1080, "Vulkan Renderer", nullptr, nullptr);
	glfwSwapInterval(0);

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height)
	{
		static_cast<Application *>(glfwGetWindowUserPointer(window))->framebuffer_resize_callback(window, width, height);
	});

	glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods)
	{
		static_cast<Application *>(glfwGetWindowUserPointer(window))->key_callback(window, key, scancode, action, mods);
	});

	HWND hwnd = glfwGetWin32Window(window);
	BOOL is_dark_mode = true;
	DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &is_dark_mode, sizeof(is_dark_mode));

	input.init(window);
	Log::Init();
	VkWrapper::init(window);
	AssetManager::init();

	init_sync_objects();
}

void Application::run()
{
	double prev_time = glfwGetTime();
	float delta_seconds = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		// Wait when queue for this frame is completed (device -> host sync)
		VkWrapper::command_buffers[current_frame].waitFence();

		// Acquire image and trigger semaphore
		uint32_t image_index;
		VkResult result = vkAcquireNextImageKHR(VkWrapper::device->logicalHandle, VkWrapper::swapchain->swapchain_handle, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &image_index);

		update(delta_seconds);

		Renderer::beginFrame(current_frame, image_index);
		BindlessResources::updateSets();
		TransientResources::update();
		updateBuffers(delta_seconds);

		// Record commands
		render(VkWrapper::command_buffers[current_frame]);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// Wait when semaphore when image will get acquired
		VkSemaphore wait_semaphores[] = {imageAvailableSemaphores[current_frame]};
		VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		VkCommandBuffer command_buffer = VkWrapper::command_buffers[current_frame].get_buffer();
		submit_info.pCommandBuffers = &command_buffer;
		// Signal semaphore when queue submitted
		VkSemaphore signal_semaphores[] = {renderFinishedSemaphores[current_frame]};
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;
		// Also trigger fence when queue completed to work with this 'currentFrame'
		CHECK_ERROR(vkQueueSubmit(VkWrapper::device->graphicsQueue, 1, &submit_info, VkWrapper::command_buffers[current_frame].get_fence()));

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		// Wait until queue submitted before present it
		present_info.pWaitSemaphores = signal_semaphores;
		VkSwapchainKHR swapChains[] = {VkWrapper::swapchain->swapchain_handle};
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swapChains;
		present_info.pImageIndices = &image_index;
		result = vkQueuePresentKHR(VkWrapper::device->presentQueue, &present_info);

		Renderer::endFrame(image_index);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized)
		{
			framebuffer_resized = false;
			recreate_swapchain();
		} else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to present swap chain image");
		}
		//vkQueueWaitIdle(VkWrapper::device->presentQueue);

		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

		delta_seconds = glfwGetTime() - prev_time;
		prev_time = glfwGetTime();
	}

	cleanup();
}

void Application::render(CommandBuffer &command_buffer)
{
	command_buffer.open();
	vkCmdResetQueryPool(command_buffer.get_buffer(), VkWrapper::device->query_pools[Renderer::getCurrentFrameInFlight()], 0, VkWrapper::device->time_stamps[Renderer::getCurrentFrameInFlight()].size());
	{
		GPU_SCOPE_FUNCTION(&command_buffer);

		// Set swapchain color image layout for writing
		VkWrapper::cmdImageMemoryBarrier(command_buffer,
										VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
										VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
										VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
										VkWrapper::swapchain->swapchain_images[Renderer::getCurrentImageIndex()], VK_IMAGE_ASPECT_COLOR_BIT);

		// Record commands (they do what they want + output to swapchain_textures)
		recordCommands(command_buffer);

		// Set swapchain color image layout for presenting
		VkWrapper::cmdImageMemoryBarrier(command_buffer,
										VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
										VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
										VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
										VkWrapper::swapchain->swapchain_images[Renderer::getCurrentImageIndex()], VK_IMAGE_ASPECT_COLOR_BIT);

	}
	command_buffer.close();
}

void Application::cleanup()
{
	Scene::closeScene();
	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
	AssetManager::shutdown();
	TransientResources::cleanup();
	cleanupResources();
	cleanup_swapchain();
	BindlessResources::cleanup();
	Shader::destroyAllShaders();
	Renderer::shutdown();
	gpu_resource_manager.cleanup();
	Renderer::deleteResources(Renderer::getCurrentFrameInFlight()); // TODO: investigate why needed after, why there is twice deletion of the same
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Renderer::deleteResources(i); // TODO: investigate why needed after, why there is twice deletion of the same
	}
	VkWrapper::shutdown();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(VkWrapper::device->logicalHandle, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(VkWrapper::device->logicalHandle, renderFinishedSemaphores[i], nullptr);
	}

	VkWrapper::device = nullptr;
	vkDestroyInstance(VkWrapper::instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::cleanup_swapchain()
{
	VkWrapper::swapchain = nullptr;
}

void Application::recreate_swapchain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	VkWrapper::swapchain->create(width, height);
	Renderer::recreateScreenResources();
	onViewportSizeChanged(width, height);
}

void Application::init_sync_objects()
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
