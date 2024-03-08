#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "Log.h"
#include "Device.h"
#include "Swapchain.h"
#include "Descriptors.h"

static const int MAX_FRAMES_IN_FLIGHT = 2;

class CommandBuffer
{
public:
	CommandBuffer()
	{}

	void init(VkDevice device, VkCommandPool command_pool)
	{
		this->device = device;
		this->command_pool = command_pool;

		// Create buffer
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		CHECK_ERROR(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));

		// Create fence
		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		CHECK_ERROR(vkCreateFence(device, &fence_info, nullptr, &in_flight_fence));
	}

	~CommandBuffer()
	{
		vkDestroyFence(device, in_flight_fence, nullptr);
	}

	void waitFence()
	{
		vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	}

	void open()
	{
		if (is_open)
			return;

		vkResetFences(device, 1, &in_flight_fence);

		VkCommandBufferBeginInfo info{};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkResetCommandBuffer(command_buffer, 0);
		vkBeginCommandBuffer(command_buffer, &info);

		is_open = true;
	}

	void close()
	{
		CHECK_ERROR(vkEndCommandBuffer(command_buffer));
		is_open = false;
	}

	VkCommandBuffer get_buffer() const
	{
		return command_buffer;
	}

	VkFence get_fence() const
	{
		return in_flight_fence;
	}

private:
	bool is_open = false;
	VkCommandBuffer command_buffer;
	VkFence in_flight_fence;

	VkDevice device;
	VkCommandPool command_pool;
};

class VkWrapper
{
public:
	static void init(GLFWwindow* window);
	static void cleanup();

	static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_property_flags, VkBuffer &buffer, VkDeviceMemory &buffer_memory)
	{
		// Create buffer
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CHECK_ERROR(vkCreateBuffer(device->logicalHandle, &info, nullptr, &buffer));

		// Get memory requirements
		VkMemoryRequirements memoryRequrements;
		vkGetBufferMemoryRequirements(device->logicalHandle, buffer, &memoryRequrements);

		// Allocate memory based on requirements
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memoryRequrements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memoryRequrements.memoryTypeBits, memory_property_flags);
		CHECK_ERROR(vkAllocateMemory(device->logicalHandle, &allocInfo, nullptr, &buffer_memory));

		// Bind allocated memory to buffer
		vkBindBufferMemory(device->logicalHandle, buffer, buffer_memory, 0);
	}

	static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		for (uint32_t i = 0; i < device->memory_properties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (device->memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
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

private:
	static void init_instance();
	static void init_command_buffers();
public:
	static VkInstance instance;
	static std::shared_ptr<Device> device;
	static std::vector<CommandBuffer> command_buffers;
	static std::shared_ptr<Swapchain> swapchain;
	static std::shared_ptr<DescriptorAllocator> global_descriptor_allocator;
};