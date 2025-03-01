#include "pch.h"
#define VMA_IMPLEMENTATION
#include "VulkanDynamicRHI.h"

#include <Rendering/Renderer.h>
#include "VulkanUtils.h"

void VulkanDynamicRHI::init()
{
	init_instance();
	device = std::make_shared<Device>(instance);
	init_vma();

	//Create Command Pool
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = device->queueFamily.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	CHECK_ERROR(vkCreateCommandPool(device->logicalHandle, &poolInfo, nullptr, &command_pool));

	global_descriptor_allocator = std::make_shared<DescriptorAllocator>();

	VulkanUtils::init();



	in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);
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
		// Create fence
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		CHECK_ERROR(vkCreateFence(device->logicalHandle, &fence_info, nullptr, &in_flight_fences[i]));

		CHECK_ERROR(vkCreateSemaphore(device->logicalHandle, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
		CHECK_ERROR(vkCreateSemaphore(device->logicalHandle, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]));
	}

	cmd_queue = new VulkanCommandQueue(device->graphicsQueue, false);
	cmd_copy_queue = new VulkanCommandQueue(device->graphicsQueue);
	cmd_list = new VulkanCommandList();
	cmd_list_immediate = new VulkanCommandList();

	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxc_utils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxc_compiler));

	dxc_utils->CreateDefaultIncludeHandler(&dxc_include_handler);

	bindless_resources = new VulkanBindlessResources();
	bindless_resources->init();
}

void VulkanDynamicRHI::shutdown()
{
	auto *bindless = bindless_resources;
	bindless_resources = nullptr;
	buffers_for_shaders.clear();
	delete bindless;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(device->logicalHandle, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device->logicalHandle, renderFinishedSemaphores[i], nullptr);
	}

	if (swapchain)
	{
		swapchain->cleanup();
		swapchain = nullptr;
	}

	for (auto fence : in_flight_fences)
	{
		vkDestroyFence(device->logicalHandle, fence, nullptr);
	}
	in_flight_fences.clear();

	cmd_queue->fence = nullptr;
	delete cmd_queue;
	delete cmd_copy_queue;

	imageAvailableSemaphores.clear();
	renderFinishedSemaphores.clear();

	for (auto &shader : cached_shaders)
	{
		auto native_shader = (VulkanShader *)shader.second.get();
		native_shader->destroy();
	}
	cached_shaders.clear();

	gGpuResourceManager.cleanup();

	releaseGPUResources(0);
	releaseGPUResources(1);

	DescriptorLayoutBuilder::clearAllCaches();
	vkDestroyCommandPool(device->logicalHandle, command_pool, nullptr);
	global_descriptor_allocator->cleanup();
	char *str = new char[5000];
	vmaBuildStatsString(allocator, &str, true);

	vmaDestroyAllocator(allocator);

	device = nullptr;
	vkDestroyInstance(instance, nullptr);
}

std::shared_ptr<RHISwapchain> VulkanDynamicRHI::createSwapchain(GLFWwindow *window)
{
	this->window = window;
	VkSurfaceKHR surface;
	glfwCreateWindowSurface(instance, window, nullptr, &surface);
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	SwapchainInfo info;
	info.width = width;
	info.height = height;
	info.format = FORMAT_R8G8B8A8_UNORM;
	info.textures_count = MAX_FRAMES_IN_FLIGHT;
	swapchain = std::make_shared<VulkanSwapchain>(surface, info);
	return swapchain;
}

void VulkanDynamicRHI::resizeSwapchain(int width, int height)
{
	gDynamicRHI->waitGPU();
	swapchain->resize(width, height);
}

std::shared_ptr<RHIShader> VulkanDynamicRHI::createShader(std::wstring path, ShaderType type, std::wstring entry_point)
{
	if (entry_point.empty())
	{
		if (type == VERTEX_SHADER)
			entry_point = L"VSMain";
		else if (type == FRAGMENT_SHADER)
			entry_point = L"PSMain";
		else if (type == COMPUTE_SHADER)
			entry_point = L"CSMain";
	}

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	if (cached_shaders.find(cache_hash) != cached_shaders.end())
	{
		return cached_shaders[cache_hash];
	}

	size_t hash;
	ComPtr<IDxcBlob> blob = compile_shader(path, type, entry_point, true, hash);
	auto shader = std::make_shared<VulkanShader>((const uint32_t *)blob->GetBufferPointer(), blob->GetBufferSize(), this, type, hash);
	cached_shaders[cache_hash] = shader;
	return shader;
}

std::shared_ptr<RHIShader> VulkanDynamicRHI::createShader(std::wstring path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines)
{
	std::wstring entry_point;
	if (type == VERTEX_SHADER)
		entry_point = L"VSMain";
	else if (type == FRAGMENT_SHADER)
		entry_point = L"PSMain";
	else if (type == COMPUTE_SHADER)
		entry_point = L"CSMain";

	size_t cache_hash = 0;
	hash_combine(cache_hash, path);
	hash_combine(cache_hash, type);
	hash_combine(cache_hash, entry_point);

	std::string all_defines = "";
	for (const auto &define : defines)
	{
		all_defines += define.first;
		all_defines += define.second;
	}
	Engine::Math::hash_combine(cache_hash, all_defines);

	if (cached_shaders.find(cache_hash) != cached_shaders.end())
	{
		return cached_shaders[cache_hash];
	}

	size_t hash;
	ComPtr<IDxcBlob> blob = compile_shader(path, type, entry_point, true, hash, &defines);
	auto shader = std::make_shared<VulkanShader>((const uint32_t *)blob->GetBufferPointer(), blob->GetBufferSize(), this, type, hash);
	cached_shaders[cache_hash] = shader;
	return shader;
}

std::shared_ptr<RHIPipeline> VulkanDynamicRHI::createPipeline()
{
	auto pipeline = std::make_shared<VulkanPipeline>();
	gGpuResourceManager.registerResource(pipeline);
	return pipeline;
}

std::shared_ptr<RHIBuffer> VulkanDynamicRHI::createBuffer(BufferDescription description)
{
	auto buffer = std::make_shared<VulkanBuffer>(description);
	gGpuResourceManager.registerResource(buffer);
	return buffer;
}

std::shared_ptr<RHITexture> VulkanDynamicRHI::createTexture(TextureDescription description)
{
	auto texture = std::make_shared<VulkanTexture>(description);
	gGpuResourceManager.registerResource(texture);
	return texture;
}

void VulkanDynamicRHI::waitGPU()
{
	vkDeviceWaitIdle(device->logicalHandle);
}

void VulkanDynamicRHI::prepareRenderCall()
{
	VulkanCommandList *cmd_list_native = static_cast<VulkanCommandList *>(getCmdList());
	VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_list_native->current_pipeline.get());
	bool pso_changed = false;
	if (native_pso != last_native_pso)
	{
		last_native_pso = native_pso;
		pso_changed = true;
	}

	bool is_something_changed = pso_changed || is_textures_dirty || is_uav_textures_dirty || is_buffers_dirty;

	VkDescriptorSet current_set;

	size_t descriptor_hash = native_pso->getHash();
	auto &all_descriptors = descriptors[descriptor_hash];

	if (is_something_changed || all_descriptors.current_offset == 0)
	{
		// If no descriptor set for this shader and offset, create it
		if (all_descriptors.descriptors.size() <= all_descriptors.current_offset)
		{
			all_descriptors.descriptors.push_back(global_descriptor_allocator->allocate(native_pso->descriptor_layout.layout));
		}

		// We get set for this layout
		current_set = all_descriptors.descriptors[all_descriptors.current_offset];
		all_descriptors.current_offset++;
	} else
	{
		current_set = *all_descriptors.descriptors.end();
	}


	// Copy binded textures descriptors to heap
	if (is_something_changed)
	{
		const auto &descriptors_info = native_pso->descriptors;
		// Update set
		DescriptorWriter writer;
		if (is_textures_dirty)
		{
			for (auto &desc : descriptors_info)
			{
				if (desc.type != DESCRIPTOR_TYPE_COMBINED_IMAGE || desc.set != 0)
					continue;

				VulkanTexture *texture = current_bind_textures[desc.binding];

				if (texture == nullptr)
					continue;

				VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				VkImageLayout image_layout = texture->isDepthTexture() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				writer.writeImage(desc.binding, descriptor_type, texture->getImageView(0, -1, false), texture->sampler, image_layout);
			}
		}

		if (is_uav_textures_dirty)
		{
			for (auto &desc : descriptors_info)
			{
				if (desc.type != DESCRIPTOR_TYPE_STORAGE_IMAGE || desc.set != 0)
					continue;

				VulkanTexture *texture = current_bind_uav_textures[desc.binding];

				if (texture == nullptr)
					continue;

				VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				VkImageLayout image_layout = VK_IMAGE_LAYOUT_GENERAL;

				writer.writeImage(desc.binding, descriptor_type, current_bind_uav_textures_views[desc.binding], texture->sampler, image_layout);
			}
		}

		if (is_buffers_dirty)
		{
			for (auto &desc : descriptors_info)
			{
				if (desc.type != DESCRIPTOR_TYPE_UNIFORM_BUFFER || desc.set != 0)
					continue;

				VulkanBuffer *buffer = current_bind_structured_buffers[desc.binding];

				if (buffer == nullptr)
					continue;

				VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

				writer.writeBuffer(desc.binding, descriptor_type, buffer->bufferHandle, buffer->description.size);
			}
		}

		writer.updateSet(current_set);
	}

	VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
	if (native_pso->description.is_compute_pipeline)
		bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

	// use offset?
	vkCmdBindDescriptorSets(cmd_list->cmd_buffer, bind_point, native_pso->pipeline_layout, 0, 1, &current_set, 0, nullptr);

	// Bind bindless
	VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
	VkDescriptorSet bindless_set = native_bindless->getDescriptorSet();
	vkCmdBindDescriptorSets(cmd_list->cmd_buffer, bind_point, native_pso->pipeline_layout, 1, 1, &bindless_set, 0, nullptr);
}

void VulkanDynamicRHI::init_instance()
{
	std::vector<const char *> extensions;
	// Needed extensions
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// GLFW extensions
	{
		uint32_t extensions_count = 0;
		const char **extensions_name = glfwGetRequiredInstanceExtensions(&extensions_count);
		for (int i = 0; i < extensions_count; i++)
			extensions.push_back(extensions_name[i]);
	}

	// Instance creation
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Application";
	appInfo.pEngineName = "Vulkan engine";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &appInfo;
	info.enabledExtensionCount = extensions.size();
	info.ppEnabledExtensionNames = extensions.data();

	#ifdef DEBUG
	std::vector<const char *> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	info.enabledLayerCount = s_ValidationLayers.size();
	info.ppEnabledLayerNames = s_ValidationLayers.data();
	#else
	info.enabledLayerCount = 0;
	info.ppEnabledLayerNames = nullptr;
	#endif

	CHECK_ERROR(vkCreateInstance(&info, nullptr, &instance));
}

void VulkanDynamicRHI::init_vma()
{
	VmaAllocatorCreateInfo allocator_create_info{};
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
	allocator_create_info.physicalDevice = device->physicalHandle;
	allocator_create_info.device = device->logicalHandle;
	allocator_create_info.instance = instance;
	allocator_create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	vmaCreateAllocator(&allocator_create_info, &allocator);
}

void VulkanDynamicRHI::beginFrame()
{
	Renderer::beginFrame(current_frame, current_frame);

	for (auto &tex : current_bind_textures)
		tex = nullptr;
	for (auto &buf : current_bind_structured_buffers)
		buf = nullptr;

	// getBackBufferImageIndex
	// Acquire image and trigger semaphore
	VkResult result = vkAcquireNextImageKHR(device->logicalHandle, swapchain->swapchain, UINT64_MAX, imageAvailableSemaphores[current_frame], VK_NULL_HANDLE, &image_index);

	vkResetFences(device->logicalHandle, 1, &in_flight_fences[current_frame]);

	// Reset offsets for uniform buffers
	for (auto &buffers : buffers_for_shaders)
	{
		buffers.second.current_offset = 0;
	}

	for (auto &desc : descriptors)
	{
		desc.second.current_offset = 0;
	}

	getCmdList()->open();
}

void VulkanDynamicRHI::endFrame()
{
	// Update bindless
	bindless_resources->updateSets();

	gDynamicRHI->getCmdList()->close();

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// Wait when semaphore when image will get acquired
	VkSemaphore wait_semaphores[] = {imageAvailableSemaphores[current_frame]};
	VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_list->cmd_buffer;
	// Signal semaphore when queue submitted
	VkSemaphore signal_semaphores[] = {renderFinishedSemaphores[current_frame]};
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;
	// Also trigger fence when queue completed to work with this 'currentFrame'

	cmd_queue->fence = in_flight_fences[current_frame];
	cmd_queue->execute(cmd_list, submit_info);

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	// Wait until queue submitted before present it
	present_info.pWaitSemaphores = signal_semaphores;
	VkSwapchainKHR swapChains[] = {swapchain->swapchain};
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapChains;
	present_info.pImageIndices = &image_index;
	VkResult result = vkQueuePresentKHR(device->presentQueue, &present_info);

	Renderer::endFrame(image_index);
	cmd_queue->wait(0);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized)
	{
		framebuffer_resized = false;

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}
		resizeSwapchain(width, height);
		Renderer::recreateScreenResources();
	} else if (result != VK_SUCCESS)
	{
		CORE_CRITICAL("Failed to present swap chain image");
	}
	//vkQueueWaitIdle(VkWrapper::device->presentQueue);

	// Wait when queue for next frame is completed (device -> host sync)
	//vkWaitForFences(VkWrapper::device->logicalHandle, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	releaseGPUResources(current_frame);

	current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanDynamicRHI::releaseGPUResources(uint32_t current_frame)
{
	if (!gpu_release_queue[current_frame].empty())
	{
		//vkDeviceWaitIdle(VkWrapper::device->logicalHandle);

		for (auto resource : gpu_release_queue[current_frame])
		{
			switch (resource.first)
			{
				case RESOURCE_TYPE_SAMPLER:
					vkDestroySampler(device->logicalHandle, (VkSampler)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE_VIEW:
					vkDestroyImageView(device->logicalHandle, (VkImageView)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE:
					vkDestroyImage(device->logicalHandle, (VkImage)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_MEMORY:
					vmaFreeMemory(allocator, (VmaAllocation)resource.second);
					break;
				case RESOURCE_TYPE_BUFFER:
					vkDestroyBuffer(device->logicalHandle, (VkBuffer)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_PIPELINE:
					vkDestroyPipeline(device->logicalHandle, (VkPipeline)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_PIPELINE_LAYOUT:
					vkDestroyPipelineLayout(device->logicalHandle, (VkPipelineLayout)resource.second, nullptr);
					break;
			}
		}

		gpu_release_queue[current_frame].clear();
	}
}
