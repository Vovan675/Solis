#pragma once
#include "vulkan/vulkan.h"
#include "vma/vk_mem_alloc.h"
#include <vector>
#include "Log.h"
#include "RHI/Device.h"
#include "RHI/Swapchain.h"
#include "RHI/Descriptors.h"
#include "RHI/Texture.h"
#include "Rendering/GlobalPipeline.h"
#include "RHI/CommandBuffer.h"

static const int MAX_FRAMES_IN_FLIGHT = 2;

class VkWrapper
{
public:
	static void init(GLFWwindow* window);
	static void cleanup();

	static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage, VkBuffer &buffer, VmaAllocation &allocation)
	{
		// Create buffer
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CHECK_ERROR(vkCreateBuffer(device->logicalHandle, &info, nullptr, &buffer));

		VmaAllocationCreateInfo alloc_info{};
		alloc_info.usage = memory_usage;

		vmaCreateBuffer(allocator, &info, &alloc_info, &buffer, &allocation, nullptr);
	}

	static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	static VkCommandBuffer beginSingleTimeCommands();
	static void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	static VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count = 1)
	{
		VkDescriptorSetLayoutBinding layout_binding{};
		layout_binding.binding = binding;
		layout_binding.descriptorCount = count;
		layout_binding.descriptorType = descriptor_type;
		layout_binding.pImmutableSamplers = nullptr;
		layout_binding.stageFlags = stage_flags;
		return layout_binding;
	}
	
	static VkDescriptorPool createDescriptorPool(uint32_t uniform_buffers_count, uint32_t samplers_count)
	{
		VkDescriptorPool descriptor_pool;
		std::vector<VkDescriptorPoolSize> poolSizes{};

		if (uniform_buffers_count != 0)
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffers_count});

		if (samplers_count != 0)
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplers_count});

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
		CHECK_ERROR(vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
		return descriptor_pool;
	}

	static VkDescriptorPool createBindlessDescriptorPool(uint32_t bindless_samplers_count)
	{
		VkDescriptorPool descriptor_pool;
		std::vector<VkDescriptorPoolSize> poolSizes{};

		if (bindless_samplers_count != 0)
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindless_samplers_count});

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // flag needed for bindless
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = bindless_samplers_count;
		CHECK_ERROR(vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
		return descriptor_pool;
	}

	static VkWriteDescriptorSet bufferWriteDescriptorSet(VkDescriptorSet set, uint32_t binding, VkDescriptorType descriptor_type, VkDescriptorBufferInfo *descriptor_buffer_info, uint32_t descriptor_count = 1)
	{
		VkWriteDescriptorSet write_descriptor_set{};
		write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set.dstSet = set;
		write_descriptor_set.dstBinding = binding;
		write_descriptor_set.dstArrayElement = 0;
		write_descriptor_set.descriptorType = descriptor_type;
		write_descriptor_set.descriptorCount = descriptor_count;
		write_descriptor_set.pBufferInfo = descriptor_buffer_info;
		write_descriptor_set.pImageInfo = nullptr;
		write_descriptor_set.pTexelBufferView = nullptr;
		return write_descriptor_set;
	}

	static VkWriteDescriptorSet imageWriteDescriptorSet(VkDescriptorSet set, uint32_t binding, VkDescriptorImageInfo *descriptor_image_info, uint32_t descriptor_count = 1)
	{
		VkWriteDescriptorSet write_descriptor_set{};
		write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set.dstSet = set;
		write_descriptor_set.dstBinding = binding;
		write_descriptor_set.dstArrayElement = 0;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_set.descriptorCount = descriptor_count;
		write_descriptor_set.pBufferInfo = nullptr;
		write_descriptor_set.pImageInfo = descriptor_image_info;
		write_descriptor_set.pTexelBufferView = nullptr;
		return write_descriptor_set;
	}

	static VkWriteDescriptorSet imageWriteBindlessDescriptorSet(VkDescriptorSet set,
																uint32_t binding,
																VkDescriptorImageInfo *descriptor_image_info,
																uint32_t array_element,
																uint32_t descriptor_count = 1)
	{
		VkWriteDescriptorSet write_descriptor_set{};
		write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_descriptor_set.dstSet = set;
		write_descriptor_set.dstBinding = binding;
		write_descriptor_set.dstArrayElement = array_element;
		write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_descriptor_set.descriptorCount = descriptor_count;
		write_descriptor_set.pBufferInfo = nullptr;
		write_descriptor_set.pImageInfo = descriptor_image_info;
		write_descriptor_set.pTexelBufferView = nullptr;
		return write_descriptor_set;
	}

	static void cmdImageMemoryBarrier(CommandBuffer &command_buffer,
		VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
		VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask,
		VkImageLayout old_layout, VkImageLayout new_layout,
		VkImage image, VkImageAspectFlags aspect_mask,
		int level_count = 1, int layer_count = 1,
		int base_level = 0, int base_layer = 0);
	static void cmdBeginRendering(CommandBuffer &command_buffer, const std::vector<std::shared_ptr<Texture>> &color_attachments, std::shared_ptr<Texture> depth_attachment, int cubemap_face = -1, int mip = 0);
	static void cmdEndRendering(CommandBuffer &command_buffer);

	static std::vector<Descriptor> getMergedDescriptors(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader);
	static DescriptorLayout getDescriptorLayout(std::vector<Descriptor> descriptors);
	static DescriptorLayout getDescriptorLayout(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader);

private:
	static void init_instance();
	static void init_vma();
	static void init_command_buffers();
public:
	static VkInstance instance;
	static std::shared_ptr<Device> device;
	static VmaAllocator allocator;
	static VkCommandPool command_pool;
	static std::vector<CommandBuffer> command_buffers;
	static std::shared_ptr<Swapchain> swapchain;
	static std::shared_ptr<DescriptorAllocator> global_descriptor_allocator;
	static std::shared_ptr<GlobalPipeline> global_pipeline;
	static std::vector<std::shared_ptr<Texture>> current_render_targets;

	// Cache for full program descriptor layouts
	static std::unordered_map<size_t, DescriptorLayout> cached_merged_descriptor_layouts;
};