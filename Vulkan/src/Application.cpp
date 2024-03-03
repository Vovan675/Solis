#include "pch.h"
#include "Application.h"
#include "VkWrapper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

static std::vector<const char *> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

struct UniformBufferObject {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};


static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto application = (Application*)(glfwGetWindowUserPointer(window));
	application->framebufferResized = true;
}

Application::Application()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); //FUCK OFF OPENGL
	window = glfwCreateWindow(800, 600, "Hello Vulkan", nullptr, nullptr);
	glfwSwapInterval(0);

	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	Log::Init();
	VkWrapper::init(window);

	InitShaders();
	InitDescriptorLayout();
	InitPipeline(); 
	InitDepth();
	InitTextureImage();
	InitMesh();
	InitUniformBuffer();
	InitDescriptorPool();
	InitDescriptorSet();
	InitSyncObjects();
	InitImgui();
}

void Application::Run()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		
		// Wait when queue for this frame is completed (device -> host sync)
		VkWrapper::command_buffers[currentFrame].waitFence();

		// Acquire image and trigger semaphore
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(VkWrapper::device->logicalHandle, VkWrapper::swapchain->swapchainHandle, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapchain();
			continue;
		}
		else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to acquire next image");
		}


		// Init Imgui
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Draw Imgui Windows
		ImGui::ShowDemoWindow();

		// Record commands
		RecordCommandBuffer(VkWrapper::command_buffers[currentFrame], imageIndex);

		// Update uniforms
		UpdateUniformBuffer(currentFrame);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		// Wait when semaphore when image will get acquired
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		VkCommandBuffer command_buffer = VkWrapper::command_buffers[currentFrame].get_buffer();
		submitInfo.pCommandBuffers = &command_buffer;
		// Signal semaphore when queue submitted
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		// Also trigger fence when queue completed to work with this 'currentFrame'
		CHECK_ERROR(vkQueueSubmit(VkWrapper::device->graphicsQueue, 1, &submitInfo, VkWrapper::command_buffers[currentFrame].get_fence()));

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		// Wait until queue submitted before present it
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { VkWrapper::swapchain->swapchainHandle };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		result = vkQueuePresentKHR(VkWrapper::device->presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
		{
			framebufferResized = false;
			RecreateSwapchain();
		}
		else if (result != VK_SUCCESS)
		{
			CORE_CRITICAL("Failed to present swap chain image");
		}
		//vkQueueWaitIdle(VkWrapper::device->presentQueue);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	cleanup();
}

void Application::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = /*glm::rotate(glm::mat4(1), time * glm::radians(45.0f), glm::vec3(0, 1, 0)) **/ glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(1, 0, 0)) * glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
	ubo.view = glm::lookAt(glm::vec3(2, -2, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	ubo.proj = glm::perspective(glm::radians(45.0f), VkWrapper::swapchain->swapExtent.width / (float)VkWrapper::swapchain->swapExtent.height, 0.1f, 60.0f);
	//ubo.proj[1][1] *= -1;
	//ubo.proj = glm::mat4(1);
	//ubo.view = glm::mat4(1);
	memcpy(uniform_buffer_mapped[currentImage], &ubo, sizeof(ubo));
}

static void CmdImageMemoryBarrier(CommandBuffer& command_buffer, VkImage image,
								  VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
								  VkImageLayout oldLayout, VkImageLayout newLayout,
								  VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask
								  )
{
	VkImageMemoryBarrier image_memory_barrier{};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

	// Can be resolved by layout (hard-coded)
	image_memory_barrier.srcAccessMask = srcAccessMask; 
	image_memory_barrier.dstAccessMask = dstAccessMask;
	
	image_memory_barrier.oldLayout = oldLayout; // Could be saved in struct
	image_memory_barrier.newLayout = newLayout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.baseMipLevel = 0;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer.get_buffer(), srcStageMask,
						 dstStageMask, 0,
						 0, nullptr,
						 0, nullptr,
						 1, &image_memory_barrier);
}

void Application::RecordCommandBuffer(CommandBuffer& command_buffer, uint32_t image_index)
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
	color_attachment_info.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };

	VkRenderingAttachmentInfo depth_stencil_attachment_info{};
	depth_stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depth_stencil_attachment_info.imageView = depthStencilImages[image_index]->imageView;
	depth_stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depth_stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depth_stencil_attachment_info.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo rendering_info{};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea.offset = {0, 0};
	rendering_info.renderArea.extent = VkWrapper::swapchain->swapExtent;
	rendering_info.layerCount = 1;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_attachment_info;
	rendering_info.pDepthAttachment = &depth_stencil_attachment_info;

	vkCmdBeginRendering(command_buffer.get_buffer(), &rendering_info);

	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
	
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)VkWrapper::swapchain->swapExtent.width;
	viewport.height = (float)VkWrapper::swapchain->swapExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer.get_buffer(), 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = VkWrapper::swapchain->swapExtent;
	vkCmdSetScissor(command_buffer.get_buffer(), 0, 1, &scissor);

	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

	VkBuffer vertexBuffers[] = { modelMesh->vertexBuffer->bufferHandle };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), modelMesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), modelMesh->indices.size(), 1, 0, 0, 0);

	// Render imgui
	// This is very fast integration
	// TODO: Think about do this in separate vkCmdBeginRendering and blit image
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.get_buffer());

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

void Application::cleanup()
{
	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
	VkWrapper::cleanup();
	depthStencilImages.clear();
	image = nullptr;
	modelMesh = nullptr;
	uniformBuffers.clear();
	CleanupSwapchain();

	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, imgui_pool, nullptr);
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplVulkan_Shutdown();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(VkWrapper::device->logicalHandle, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(VkWrapper::device->logicalHandle, renderFinishedSemaphores[i], nullptr);
	}
	VkWrapper::command_buffers.clear();

	pipeline = nullptr;

	VkWrapper::device = nullptr;
	//vkDestroySurfaceKHR(VkWrapper::instance, VkWrapper::swapchain->m_Surface, nullptr);
	vkDestroyInstance(VkWrapper::instance, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

void Application::CleanupSwapchain()
{
	VkWrapper::swapchain = nullptr;
}

void Application::RecreateSwapchain()
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
	InitDepth();
}

void Application::InitImgui()
{
	// create descriptor pool for IMGUI
	VkDescriptorPoolSize pool_sizes[] = {{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &pool_info, nullptr, &imgui_pool);

	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = VkWrapper::instance;
	init_info.PhysicalDevice = VkWrapper::device->physicalHandle;
	init_info.Device = VkWrapper::device->logicalHandle;
	init_info.QueueFamily = VkWrapper::device->queueFamily.graphicsFamily.value();
	init_info.Queue = VkWrapper::device->graphicsQueue;
	//init_info.PipelineCache = g_PipelineCache;
	init_info.DescriptorPool = imgui_pool;
	//init_info.RenderPass = wd->RenderPass;
	//init_info.Subpass = 0;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;

	VkPipelineRenderingCreateInfo pipeline_rendering_create_info{};
	pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipeline_rendering_create_info.colorAttachmentCount = 1;
	pipeline_rendering_create_info.pColorAttachmentFormats = &VkWrapper::swapchain->surfaceFormat.format;
	pipeline_rendering_create_info.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
	init_info.PipelineRenderingCreateInfo = pipeline_rendering_create_info;
	//init_info.Allocator = g_Allocator;
	//init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info);

	//ImGui_ImplVulkan_CreateFontsTexture();
}

void Application::InitShaders()
{
	vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle , "shaders/simple.vert", Shader::VERTEX_SHADER);
	fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.frag", Shader::FRAGMENT_SHADER);
}

void Application::InitDescriptorLayout()
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> bindings = std::vector<VkDescriptorSetLayoutBinding> { samplerLayoutBinding, uboLayoutBinding };
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();

	CHECK_ERROR(vkCreateDescriptorSetLayout(VkWrapper::device->logicalHandle, &info, nullptr, &descriptorSetLayout));
}

void Application::InitPipeline()
{
	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;
	description.descriptor_set_layout = &descriptorSetLayout;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);

	vertShader = nullptr;
	fragShader = nullptr;
}

void Application::InitMesh()
{
	modelMesh = std::make_shared<Engine::Mesh>("assets/model2.obj");
}

void Application::InitDepth()
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
		depthStencilImages[i]->Fill();
	}
}

void Application::InitTextureImage()
{
	int texWidth, texHeight, texChannels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* pixels = stbi_load("assets/albedo2.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	
	if (!pixels)
	{
		CORE_ERROR("Loading texture error");
	}

	TextureDescription description;
	description.width = texWidth;
	description.height = texHeight;
	description.mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
	description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image = std::make_shared<Texture>(description);
	image->Fill(pixels);

	stbi_image_free(pixels);
}

void Application::InitUniformBuffer()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniform_buffer_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		uniformBuffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		uniformBuffers[i]->Map(&uniform_buffer_mapped[i]);
	}
}

void Application::InitDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	CHECK_ERROR(vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &poolInfo, nullptr, &descriptorPool));
}

void Application::InitDescriptorSet()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();
	
	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	CHECK_ERROR(vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &allocInfo, descriptorSets.data()));

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i]->bufferHandle;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = image->imageView;
		imageInfo.sampler = image->sampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr;
		descriptorWrites[0].pTexelBufferView = nullptr;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = nullptr;
		descriptorWrites[1].pImageInfo = &imageInfo;
		descriptorWrites[1].pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(VkWrapper::device->logicalHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}
}

void Application::InitSyncObjects()
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

