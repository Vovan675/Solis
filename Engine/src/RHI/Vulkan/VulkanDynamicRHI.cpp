#include "pch.h"
#define VMA_IMPLEMENTATION
#include "VulkanDynamicRHI.h"

#include <Rendering/Renderer.h>
#include "VulkanUtils.h"
#include "TracyVulkan.hpp"
#include "Core/Variables.h"
#include "RHI/StreamlineWrapper.h"

static VkBool32 validation_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user_data)
{
	// Statistics query seems to work okay even with mesh shaders at the same time
	if (data->pMessageIdName && strstr(data->pMessageIdName, "pipelineStatistics-07076"))
		return VK_FALSE;

	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		CORE_ERROR("{}", data->pMessage);
	} else
	{
		CORE_WARN("{}", data->pMessage);
	}

	OutputDebugStringA(data->pMessage);
	OutputDebugStringA("\n");

	if (engine_rhi_validation_break)
		__debugbreak();
	return VK_FALSE;
}

void VulkanDynamicRHI::init()
{
	PROFILE_CPU_FUNCTION();
	if (engine_streamline)
		StreamlineWrapper::init();

	init_instance();
	device = new Device(instance);
	init_vma();

	//Create Command Pool
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = device->queueFamily.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	CHECK_ERROR(vkCreateCommandPool(device->logicalHandle, &poolInfo, nullptr, &command_pool));

	global_descriptor_allocator = std::make_shared<DescriptorAllocator>();

	VulkanUtils::init();

	#ifdef ENABLE_RHI_VALIDATION
		if (engine_rhi_validation)
		{
			VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
			messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			messenger_info.pfnUserCallback = validation_callback;
			CHECK_ERROR(VulkanUtils::vkCreateDebugUtilsMessengerEXT(&messenger_info, nullptr, &debug_messenger));
		}
	#endif

	if (engine_streamline)
	{
		streamline.init();
		dlss_upscaler.init();
	}

	cmd_queue = new VulkanCommandQueue(device->graphicsQueue);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		cmd_lists[i] = new VulkanCommandList();
		upload_cmd_lists[i] = new VulkanCommandList();
	}
	tracy_cmd_list = new VulkanCommandList();

	bindless_resources = new VulkanBindlessResources();
	bindless_resources->init();

	tracy_ctx = TracyVkContextCalibrated(device->physicalHandle, device->logicalHandle, cmd_queue->queue, tracy_cmd_list->cmd_buffer,
										 VulkanUtils::p_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, VulkanUtils::p_vkGetCalibratedTimestampsEXT);
}

void VulkanDynamicRHI::shutdown()
{
	TracyVkDestroy(tracy_ctx);

	dlss_upscaler.shutdown();
	StreamlineWrapper::shutdown();

	release_gpu_resources(UINT64_MAX);
	gDynamicRHI->getBindlessResources()->cleanup();

	auto *bindless = bindless_resources;
	bindless_resources = nullptr;
	buffers_for_shaders.clear();
	delete bindless;

	if (swapchain)
	{
		swapchain->cleanup();
		swapchain = nullptr;
	}

	delete cmd_queue;

	for (auto &shader : cached_shaders)
	{
		auto native_shader = (VulkanShader *)shader.second.getReference();
		native_shader->destroy();
	}
	cached_shaders.clear();

	release_gpu_resources(UINT64_MAX);

	DescriptorLayoutBuilder::clearAllCaches();
	vkDestroyCommandPool(device->logicalHandle, command_pool, nullptr);
	global_descriptor_allocator->cleanup();
	vmaDestroyAllocator(allocator);

	device = nullptr;
	if (debug_messenger)
		VulkanUtils::vkDestroyDebugUtilsMessengerEXT(debug_messenger, nullptr);
	vkDestroyInstance(instance, nullptr);
}

RHISwapchainRef VulkanDynamicRHI::createSwapchain(GLFWwindow *window)
{
	VkSurfaceKHR surface;
	glfwCreateWindowSurface(instance, window, nullptr, &surface);
	int width, height;
	glfwGetWindowSize(window, &width, &height);

	SwapchainDescription info;
	info.width = width;
	info.height = height;
	info.format = Format::R8G8B8A8_UNORM;
	info.texture_count = MAX_FRAMES_IN_FLIGHT;
	swapchain = new VulkanSwapchain(surface, info);
	return swapchain;
}

void VulkanDynamicRHI::resizeSwapchain(int width, int height)
{
	swapchain->resize(width, height);
}

RHIShader *VulkanDynamicRHI::create_shader(eastl::wstring path, ShaderType type, eastl::string entry_point, eastl::vector<eastl::pair<const char *, const char *>> defines)
{
	return new VulkanShader(path, type, entry_point, defines);
}

RHIBuffer *VulkanDynamicRHI::create_buffer(const BufferDescription &description)
{
	return new VulkanBuffer(description);
}

RHIPipelineRef VulkanDynamicRHI::createPipeline(const PipelineDescription &description)
{
	return new VulkanPipeline(description);
}

RHITextureRef VulkanDynamicRHI::createTexture(const TextureDescription &description, const void *initial_data)
{
	return new VulkanTexture(description, initial_data);
}

RHIBottomLevelAccelerationStructureRef VulkanDynamicRHI::createBottomLevelAccelerationStructure()
{
	return new VulkanBottomLevelAccelerationStructure();
}

RHITopLevelAccelerationStructureRef VulkanDynamicRHI::createTopLevelAccelerationStructure()
{
	return new VulkanTopLevelAccelerationStructure();
}

RHICommandList *VulkanDynamicRHI::getUploadCmdList()
{
	VulkanCommandList *cmd_list = upload_cmd_lists[frame_in_flight];
	cmd_list->open();
	return cmd_list;
}

void VulkanDynamicRHI::waitGPU()
{
	PROFILE_CPU_FUNCTION();
	vkDeviceWaitIdle(device->logicalHandle);
}

void VulkanDynamicRHI::prepareRenderCall()
{
	PROFILE_CPU_FUNCTION();
	VulkanCommandList *cmd_list_native = static_cast<VulkanCommandList *>(getCmdList());
	VulkanPipeline *native_pso = static_cast<VulkanPipeline *>(cmd_list_native->getCurrentPipeline());
	bool pso_changed = false;
	if (native_pso != last_native_pso)
	{
		last_native_pso = native_pso;
		pso_changed = true;
	}

	bool is_something_changed = pso_changed || is_buffers_dirty;

	VkDescriptorSet current_set = nullptr;

	size_t descriptor_hash = native_pso->getHash();
	hashCombine(descriptor_hash, frame_in_flight);
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
		current_set = all_descriptors.descriptors[all_descriptors.descriptors.size() - 1];
	}


	// Copy binded textures descriptors to heap
	if (is_something_changed)
	{
		const auto &descriptors_info = native_pso->descriptors;
		// Update set
		static DescriptorWriter writer;
		writer.clear();

		if (is_buffers_dirty || true)
		{
			for (auto &desc : descriptors_info)
			{
				if (desc.type != DescriptorType::UNIFORM_BUFFER || desc.set != 0)
					continue;

				VulkanBuffer *buffer = (VulkanBuffer *)current_bind_buffers[desc.binding];

				if (buffer == nullptr)
					continue;

				VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

				writer.writeBuffer(desc.binding, descriptor_type, buffer->getBuffer(), buffer->getSize());
			}
		}

		writer.updateSet(current_set);
	}

	VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
	if (native_pso->description.pipeline_type == PipelineType::COMPUTE)
		bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
	else if (native_pso->description.pipeline_type == PipelineType::RAY_TRACING)
		bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
	VulkanCommandList *native_cmd_list = cmd_lists[frame_in_flight];
	// use offset?
	vkCmdBindDescriptorSets(native_cmd_list->cmd_buffer, bind_point, native_pso->resource->pipeline_layout, 0, 1, &current_set, 0, nullptr);

	// Bind bindless
	VulkanBindlessResources *native_bindless = (VulkanBindlessResources *)gDynamicRHI->getBindlessResources();
	VkDescriptorSet bindless_set = native_bindless->getDescriptorSet();
	vkCmdBindDescriptorSets(native_cmd_list->cmd_buffer, bind_point, native_pso->resource->pipeline_layout, BINDLESS_RESOURCES_SET, 1, &bindless_set, 0, nullptr);

	is_buffers_dirty = false;
}

void VulkanDynamicRHI::init_instance()
{
	eastl::vector<const char *> extensions;
	// Needed extensions
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// GLFW extensions
	{
		uint32_t extensions_count = 0;
		const char **extensions_name = glfwGetRequiredInstanceExtensions(&extensions_count);
		for (int i = 0; i < extensions_count; i++)
			extensions.push_back(extensions_name[i]);
	}

	VulkanStreamline::appendInstanceExtensions(extensions);

	// Instance creation
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Solis";
	appInfo.pEngineName = "Solis";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &appInfo;
	info.enabledExtensionCount = extensions.size();
	info.ppEnabledExtensionNames = extensions.data();

	#ifdef ENABLE_RHI_VALIDATION
	eastl::vector<const char *> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	if (engine_rhi_validation)
	{
		info.enabledLayerCount = s_ValidationLayers.size();
		info.ppEnabledLayerNames = s_ValidationLayers.data();
	}
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
	PROFILE_CPU_FUNCTION();

	for (auto &buf : current_bind_buffers)
		buf = nullptr;

	swapchain->acquire();
	Renderer::beginFrame();

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

	VulkanCommandList *native_cmd_list = (VulkanCommandList *)gDynamicRHI->getCmdList();
	TracyVkCollect(VulkanUtils::getNativeRHI()->tracy_ctx, native_cmd_list->cmd_buffer);

	// Begin statistics query
	VkQueryPool stats_pool = device->pipeline_statistics_query_pools[frame_in_flight];
	vkCmdResetQueryPool(native_cmd_list->cmd_buffer, stats_pool, 0, 1);
	vkCmdBeginQuery(native_cmd_list->cmd_buffer, stats_pool, 0, 0);
}

void VulkanDynamicRHI::endFrame()
{
	PROFILE_CPU_FUNCTION();
	// Update bindless
	bindless_resources->updateSets();
	VulkanCommandList *native_cmd_list = cmd_lists[frame_in_flight];
	VulkanCommandList *native_upload_cmd_list = upload_cmd_lists[frame_in_flight];

	// End statistics query
	VkQueryPool stats_pool = device->pipeline_statistics_query_pools[frame_in_flight];
	vkCmdEndQuery(native_cmd_list->cmd_buffer, stats_pool, 0);

	gDynamicRHI->getCmdList()->close();

	eastl::vector<RHICommandList *> submit_cmd_lists;
	if (native_upload_cmd_list->isOpen())
	{
		// Wait for all transfer operations (batched for all tranfer operations in this frame)
		VulkanUtils::cmdMemoryBarrier(native_upload_cmd_list->cmd_buffer,
			VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT);
		native_upload_cmd_list->close();
		submit_cmd_lists.push_back(native_upload_cmd_list);
	}
	submit_cmd_lists.push_back(native_cmd_list);

	uint64_t frame_fence_value = cmd_queue->execute(submit_cmd_lists, swapchain->getImageAcquiredSemaphore(), swapchain->getRenderingDoneSemaphore());

	swapchain->present();
	Renderer::endFrame(swapchain->getCurrentIndex());

	advance_frame_in_flight(frame_fence_value);

	if (frame >= MAX_FRAMES_IN_FLIGHT)
	{
		VkQueryPool prev_stats_pool = device->pipeline_statistics_query_pools[frame_in_flight];
		uint64_t query_results[7];
		VkResult result_query = vkGetQueryPoolResults(device->logicalHandle, prev_stats_pool, 0, 1, sizeof(query_results), query_results, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

		if (result_query == VK_SUCCESS)
		{
			gpu_statistics.input_assembly_vertices = query_results[0];
			gpu_statistics.input_assembly_primitives = query_results[1];
			gpu_statistics.vertex_shader_invocations = query_results[2];
			gpu_statistics.clipping_invocations = query_results[3];
			gpu_statistics.clipping_primitives = query_results[4];
			gpu_statistics.fragment_shader_invocations = query_results[5];
			gpu_statistics.compute_shader_invocations = query_results[6];
			//gpu_statistics.mesh_shader_invocations = query_results[7];
		}
	}

	release_gpu_resources(frame);
	frame++;
}

