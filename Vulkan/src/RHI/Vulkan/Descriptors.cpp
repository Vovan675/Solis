#include "pch.h"
#include <algorithm>
#include <functional>
#include "Descriptors.h"
#include "Math.h"
#include "EngineMath.h"
#include "VulkanUtils.h"
#include "VulkanDynamicRHI.h"

using namespace Engine::Math;

std::unordered_map<size_t, DescriptorLayout> DescriptorLayoutBuilder::cached_descriptor_layouts;

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type, uint32_t count)
{
	VkDescriptorSetLayoutBinding layout_binding{};
	layout_binding.binding = binding;
	layout_binding.descriptorCount = count;
	layout_binding.descriptorType = type;
	bindings.push_back(layout_binding);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

void DescriptorLayoutBuilder::clearAllCaches()
{
	for (auto &desc : cached_descriptor_layouts)
		vkDestroyDescriptorSetLayout(VulkanUtils::getNativeRHI()->device->logicalHandle, desc.second.layout, nullptr);
	cached_descriptor_layouts.clear();
}

DescriptorLayout DescriptorLayoutBuilder::build(VkShaderStageFlags stages, const void *pNext, VkDescriptorSetLayoutCreateFlags flags)
{
	for (auto& b : bindings)
		b.stageFlags |= stages;
	
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.pBindings = bindings.data();
	info.bindingCount = (uint32_t)bindings.size();
	info.flags = flags;

	info.pNext = pNext;

	size_t hash = 0;
	hash_combine(hash, bindings.size());
	for (const auto &b : bindings)
	{
		size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
		hash_combine(hash, binding_hash);
	}

	// Try to find cached
	auto cached_layout = cached_descriptor_layouts.find(hash);
	if (cached_layout != cached_descriptor_layouts.end())
	{
		return cached_layout->second;
	}

	VkDescriptorSetLayout set;
	CHECK_ERROR(vkCreateDescriptorSetLayout(VulkanUtils::getNativeRHI()->device->logicalHandle, &info, nullptr, &set));
	DescriptorLayout new_layout = {set, hash};
	cached_descriptor_layouts[hash] = new_layout;
	return new_layout;
}


void DescriptorAllocator::cleanup()
{
	// Descriptor sets will implicitly free
	for (auto p : free_pools)
		vkDestroyDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, p, nullptr);
	for (auto p : used_pools)
		vkDestroyDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, p, nullptr);
}

void DescriptorAllocator::resetPools()
{
	for (auto p : used_pools)
	{
		vkResetDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, p, 0);
		free_pools.push_back(p);
	}

	used_pools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout)
{
	// Get pool from empty pools
	VkDescriptorPool pool = get_pool();

	// Create descriptor set
	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	VkDescriptorSet set;
	VkResult result = vkAllocateDescriptorSets(VulkanUtils::getNativeRHI()->device->logicalHandle, &alloc_info, &set);

	if (result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY)
	{
		// Error occurs when pool can't handle more sets, so make it 'used'
		used_pools.push_back(pool);

		pool = get_pool();
		alloc_info.descriptorPool = pool;

		vkAllocateDescriptorSets(VulkanUtils::getNativeRHI()->device->logicalHandle, &alloc_info, &set);
	}

	// Return to empty pools when completed to work with
	free_pools.push_back(pool);
	return set;
}

VkDescriptorPool DescriptorAllocator::get_pool()
{
	if (free_pools.size() > 0)
	{
		VkDescriptorPool pool = free_pools.back();
		free_pools.pop_back();
		return pool;
	} else
	{
		return create_pool();
	}
}

VkDescriptorPool DescriptorAllocator::create_pool()
{
	VkDescriptorPool descriptor_pool;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.poolSizeCount = POOL_SIZES.size();
	poolInfo.pPoolSizes = POOL_SIZES.data();
	poolInfo.maxSets = 1000; // how many sets can be allocated from this pool
	CHECK_ERROR(vkCreateDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
	return descriptor_pool;
}

void DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset)
{
	VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back();
	buffer_info.buffer = buffer;
	buffer_info.offset = offset;
	buffer_info.range = size;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = nullptr; // Set later
	write.dstBinding = binding;
	write.descriptorType = type;
	write.descriptorCount = 1;
	write.pBufferInfo = &buffer_info;
	writes.push_back(write);
}

void DescriptorWriter::writeImage(uint32_t binding, VkDescriptorType type, VkImageView image, VkSampler sampler, VkImageLayout image_layout)
{
	VkDescriptorImageInfo &image_info = image_infos.emplace_back();
	image_info.imageView = image;
	image_info.sampler = sampler;
	image_info.imageLayout = image_layout;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = nullptr; // Set later
	write.dstBinding = binding;
	write.descriptorType = type;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;
	writes.push_back(write);
}

void DescriptorWriter::writeAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR *acceleration_structure)
{
	VkWriteDescriptorSetAccelerationStructureKHR &write_acceleration = acceleration_structure_infos.emplace_back();
	write_acceleration.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	write_acceleration.accelerationStructureCount = 1;
	write_acceleration.pAccelerationStructures = acceleration_structure;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = nullptr; // Set later
	write.dstBinding = binding;
	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	write.descriptorCount = 1;
	write.pNext = &write_acceleration;
	writes.push_back(write);
}

void DescriptorWriter::clear()
{
	buffer_infos.clear();
	image_infos.clear();
	acceleration_structure_infos.clear();
	writes.clear();
}

void DescriptorWriter::updateSet(VkDescriptorSet set)
{
	if (writes.empty())
		return;

	for (VkWriteDescriptorSet &write : writes)
		write.dstSet = set;

	vkUpdateDescriptorSets(VulkanUtils::getNativeRHI()->device->logicalHandle, writes.size(), writes.data(), 0, nullptr);
}
