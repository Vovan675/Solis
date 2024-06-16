#include "pch.h"
#define VMA_IMPLEMENTATION
#include "VkWrapper.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

VkInstance VkWrapper::instance;
std::shared_ptr<Device> VkWrapper::device;
VmaAllocator VkWrapper::allocator;
VkCommandPool VkWrapper::command_pool;
std::vector<CommandBuffer> VkWrapper::command_buffers;
std::shared_ptr<Swapchain> VkWrapper::swapchain;
std::shared_ptr<DescriptorAllocator> VkWrapper::global_descriptor_allocator;
std::shared_ptr<GlobalPipeline> VkWrapper::global_pipeline;
std::vector<std::shared_ptr<Texture>> VkWrapper::current_render_targets = {};

std::unordered_map<size_t, DescriptorLayout> VkWrapper::cached_merged_descriptor_layouts;


static std::vector<const char *> s_ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
};

void VkWrapper::init(GLFWwindow *window)
{
	init_instance();
	device = std::make_shared<Device>(instance);
	init_vma();
	init_command_buffers();

	VkSurfaceKHR surface;
	CHECK_ERROR(glfwCreateWindowSurface(instance, window, nullptr, &surface));
	swapchain = std::make_shared<Swapchain>(surface);
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	swapchain->create(width, height);

	global_descriptor_allocator = std::make_shared<DescriptorAllocator>();
	global_pipeline = std::make_shared<GlobalPipeline>();
	BindlessResources::init();

	Renderer::init();
}

void VkWrapper::cleanup()
{
	BindlessResources::cleanup();
	vkDestroyCommandPool(device->logicalHandle, command_pool, nullptr);
	global_descriptor_allocator->cleanup();
	vmaDestroyAllocator(allocator);
}

void VkWrapper::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	// Also i can create separate command pool for these short-living commands
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	CHECK_ERROR(vkAllocateCommandBuffers(device->logicalHandle, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->graphicsQueue);
	
	vkFreeCommandBuffers(device->logicalHandle, command_pool, 1, &commandBuffer);
}

VkCommandBuffer VkWrapper::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = command_pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device->logicalHandle, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VkWrapper::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->graphicsQueue);

	vkFreeCommandBuffers(device->logicalHandle, command_pool, 1, &commandBuffer);
}

void VkWrapper::cmdImageMemoryBarrier(CommandBuffer &command_buffer, 
									  VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
									  VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask,
									  VkImageLayout old_layout, VkImageLayout new_layout,
									  VkImage image, VkImageAspectFlags aspect_mask,
									  int level_count, int layer_count,
									  int base_level, int base_layer)
{
	VkImageMemoryBarrier2 image_memory_barrier{};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	image_memory_barrier.srcStageMask = src_stage_mask; // pipeline stage(s) that must be completed before the barrier is crossed
	image_memory_barrier.srcAccessMask = src_access_mask; // operations that must complete before the barrier is crossed - example: write
	image_memory_barrier.dstStageMask = dst_stage_mask; // pipeline stage(s) that must wait for the barrier to be crossed before beginning
	image_memory_barrier.dstAccessMask = dst_access_mask; // operations that must wait for the barrier to be is crossed - example: read
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange.aspectMask = aspect_mask;
	image_memory_barrier.subresourceRange.baseMipLevel = base_level;
	image_memory_barrier.subresourceRange.levelCount = level_count;
	image_memory_barrier.subresourceRange.baseArrayLayer = base_layer;
	image_memory_barrier.subresourceRange.layerCount = layer_count;

	VkDependencyInfo dependency_info{};
	dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency_info.imageMemoryBarrierCount = 1;
	dependency_info.pImageMemoryBarriers = &image_memory_barrier;

	vkCmdPipelineBarrier2(command_buffer.get_buffer(), &dependency_info);
}

void VkWrapper::cmdBeginRendering(CommandBuffer &command_buffer, const std::vector<std::shared_ptr<Texture>> &color_attachments, std::shared_ptr<Texture> depth_attachment, int cubemap_face, int mip)
{
	VkExtent2D extent;
	extent.width = color_attachments[0]->getWidth(mip);
	extent.height = color_attachments[0]->getHeight(mip);

	VkRenderingInfo rendering_info{};
	rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	rendering_info.renderArea.offset = {0, 0};
	rendering_info.renderArea.extent = extent;
	rendering_info.layerCount = 1;

	std::vector<VkRenderingAttachmentInfo> color_attachments_info;
	for (const auto &attachment : color_attachments)
	{
		VkRenderingAttachmentInfo info{};
		info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		info.imageView = attachment->getImageView(mip, cubemap_face);
		info.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		info.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		color_attachments_info.push_back(info);
	}
	rendering_info.colorAttachmentCount = color_attachments_info.size();
	rendering_info.pColorAttachments = color_attachments_info.data();

	VkRenderingAttachmentInfo depth_stencil_attachment_info{};
	if (depth_attachment != nullptr)
	{
		depth_stencil_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth_stencil_attachment_info.imageView = depth_attachment->getImageView();
		depth_stencil_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth_stencil_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_stencil_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_stencil_attachment_info.clearValue.depthStencil.depth = 1.0f;
		depth_stencil_attachment_info.clearValue.depthStencil.stencil = 0.0f;
		rendering_info.pDepthAttachment = &depth_stencil_attachment_info;
	}

	vkCmdBeginRendering(command_buffer.get_buffer(), &rendering_info);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = extent.width;
	viewport.height = extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer.get_buffer(), 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = extent;
	vkCmdSetScissor(command_buffer.get_buffer(), 0, 1, &scissor);

	current_render_targets = color_attachments;
	if (depth_attachment != nullptr)
		current_render_targets.push_back(depth_attachment);
}

void VkWrapper::cmdEndRendering(CommandBuffer &command_buffer)
{
	vkCmdEndRendering(command_buffer.get_buffer());
	current_render_targets.clear();
}

std::vector<Descriptor> VkWrapper::getMergedDescriptors(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader)
{
	auto vs_descriptors = vertex_shader->getDescriptors();
	auto fs_descriptors = fragment_shader->getDescriptors();

	auto merged_descriptors = vs_descriptors;

	for (int i = 0; i < fs_descriptors.size(); i++)
	{
		auto &other = fs_descriptors[i];

		bool is_found = false;

		for (auto &result : merged_descriptors)
		{
			// If this descriptor exists in other descriptor, then just merge their stages
			if (result.type == other.type && result.set == other.set && result.binding == other.binding && result.first_member_offset == other.first_member_offset)
			{
				result.stage = (DescriptorStage)(result.stage | other.stage);
				is_found = true;
				break;
			}
		}

		// This is unique descriptor
		if (!is_found)
		{
			merged_descriptors.push_back(other);
		}
	}

	return merged_descriptors;
}

DescriptorLayout VkWrapper::getDescriptorLayout(std::vector<Descriptor> descriptors)
{
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	for (const auto &descriptor : descriptors)
	{
		// Skip bindless textures
		if ((descriptor.type == DESCRIPTOR_TYPE_SAMPLER && descriptor.set == 1 && descriptor.binding == 0) // Bindless Textures
			|| (descriptor.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER && descriptor.set == 2 && descriptor.binding == 0) // Default uniforms
			)
			continue;

		VkDescriptorType descriptor_type;
		if (descriptor.type == DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		else if (descriptor.type == DESCRIPTOR_TYPE_SAMPLER)
			descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		else
			continue;
		layout_builder.add_binding(descriptor.binding, descriptor_type);
	}
	return layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
}

DescriptorLayout VkWrapper::getDescriptorLayout(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader)
{
	size_t hash = 0;
	hash_combine(hash, vertex_shader->getHash());
	hash_combine(hash, fragment_shader->getHash());

	// Try to find cached
	auto cached_layout = cached_merged_descriptor_layouts.find(hash);
	if (cached_layout != cached_merged_descriptor_layouts.end())
	{
		return cached_layout->second;
	}

	DescriptorLayout new_layout = getDescriptorLayout(getMergedDescriptors(vertex_shader, fragment_shader));
	cached_merged_descriptor_layouts[hash] = new_layout;
	return new_layout;
}


void VkWrapper::init_instance()
{
	uint32_t extensionsCount = 0;
	const char **extensionsName = glfwGetRequiredInstanceExtensions(&extensionsCount);

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.pEngineName = "Hello engine";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &appInfo;
	info.enabledExtensionCount = extensionsCount;
	info.ppEnabledExtensionNames = extensionsName;


	info.enabledLayerCount = s_ValidationLayers.size();
	info.ppEnabledLayerNames = s_ValidationLayers.data();
	CHECK_ERROR(vkCreateInstance(&info, nullptr, &instance));
}

void VkWrapper::init_vma()
{
	VmaAllocatorCreateInfo allocator_create_info{};
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_3;
	allocator_create_info.physicalDevice = device->physicalHandle;
	allocator_create_info.device = device->logicalHandle;
	allocator_create_info.instance = instance;
	
	vmaCreateAllocator(&allocator_create_info, &allocator);
}

void VkWrapper::init_command_buffers()
{
	//Create Command Pool
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = device->queueFamily.graphicsFamily.value();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	CHECK_ERROR(vkCreateCommandPool(device->logicalHandle, &poolInfo, nullptr, &command_pool))

	//Create Command buffers
	command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < command_buffers.size(); i++)
	{
		command_buffers[i].init();
	}
}