#include "pch.h"
#include <algorithm>
#include <functional>

#include "Descriptors.h"
#include "VkWrapper.h"
#include "MathUtils.h"


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
	// TODO: caching

	VkDescriptorSetLayout set;
	CHECK_ERROR(vkCreateDescriptorSetLayout(VkWrapper::device->logicalHandle, &info, nullptr, &set));
	return {set, hash};
}


void DescriptorAllocator::cleanup()
{
	// Descriptor sets will implicitly free
	for (auto p : free_pools)
		vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, p, nullptr);
	for (auto p : used_pools)
		vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, p, nullptr);
}

void DescriptorAllocator::resetPools()
{
	for (auto p : used_pools)
	{
		vkResetDescriptorPool(VkWrapper::device->logicalHandle, p, 0);
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
	VkResult result = vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &alloc_info, &set);

	if (result == VK_ERROR_FRAGMENTED_POOL || result == VK_ERROR_OUT_OF_POOL_MEMORY)
	{
		// Error occurs when pool can't handle more sets, so make it 'used'
		used_pools.push_back(pool);

		pool = get_pool();
		alloc_info.descriptorPool = pool;

		vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &alloc_info, &set);
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
	CHECK_ERROR(vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
	return descriptor_pool;
}

void DescriptorLayoutCache::cleanup()
{
	for (auto l : layout_cache)
		vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, l.second, nullptr);
}

VkDescriptorSetLayout DescriptorLayoutCache::createDescriptorLayout(VkDescriptorSetLayoutCreateInfo *info)
{
	DescriptorLayoutInfo layout_info;
	layout_info.bindings.reserve(info->bindingCount);

	// Is bindings sorted
	bool is_sorted = true;
	int last_binding = -1;

	for (int i = 0; i < info->bindingCount; i++)
	{
		layout_info.bindings.push_back(info->pBindings[i]);

		if (info->pBindings[i].binding > last_binding)
			last_binding = info->pBindings[i].binding;
		else
			is_sorted = false;
	}

	if (!is_sorted)
	{
		std::sort(layout_info.bindings.begin(), layout_info.bindings.end(), [](VkDescriptorSetLayoutBinding &a, VkDescriptorSetLayoutBinding &b)
		{
			return a.binding < b.binding;
		});
	}

	// find in cache
	auto it = layout_cache.find(layout_info);
	if (it != layout_cache.end())
		return it->second;

	// otherwise, really create it
	VkDescriptorSetLayout layout;
	CHECK_ERROR(vkCreateDescriptorSetLayout(VkWrapper::device->logicalHandle, info, nullptr, &layout));

	// cache it
	layout_cache[layout_info] = layout;
	return layout;
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo &other) const
{
	if (other.bindings.size() != bindings.size())
		return false;

	for (size_t i = 0; i < bindings.size(); i++)
	{
		if (other.bindings[i].binding != bindings[i].binding)
			return false;
		if (other.bindings[i].descriptorType != bindings[i].descriptorType)
			return false;
		if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
			return false;
		if (other.bindings[i].stageFlags != bindings[i].stageFlags)
			return false;
	}

	return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
	size_t result = std::hash<size_t>()(bindings.size());

	for (const auto &b : bindings)
	{
		size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
		result ^= std::hash<size_t>()(binding_hash);
	}
	return result;
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache *layout_cache, DescriptorAllocator *allocator)
{
	DescriptorBuilder builder;
	builder.cache = layout_cache;
	builder.allocator = allocator;
	return builder;
}

DescriptorBuilder &DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, VkDescriptorBufferInfo *buffer_info)
{
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding layout_binding{};
	layout_binding.binding = binding;
	layout_binding.descriptorCount = 1;
	layout_binding.descriptorType = descriptor_type;
	layout_binding.pImmutableSamplers = nullptr;
	layout_binding.stageFlags = stage_flags;

	bindings.push_back(layout_binding);

	// Create descriptor write
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = descriptor_type;
	write.pBufferInfo = buffer_info;
	write.dstBinding = binding;

	writes.push_back(write);

	return *this;
}

DescriptorBuilder &DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, VkDescriptorImageInfo *image_info)
{
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding layout_binding{};
	layout_binding.binding = binding;
	layout_binding.descriptorCount = 1;
	layout_binding.descriptorType = descriptor_type;
	layout_binding.pImmutableSamplers = nullptr;
	layout_binding.stageFlags = stage_flags;

	bindings.push_back(layout_binding);

	// Create descriptor write
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorCount = 1;
	write.descriptorType = descriptor_type;
	write.pImageInfo = image_info;
	write.dstBinding = binding;

	writes.push_back(write);

	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet &set, VkDescriptorSetLayout &layout)
{
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();

	layout = cache->createDescriptorLayout(&info);
	
	set = allocator->allocate(layout);
	if (set == VK_NULL_HANDLE)
		return false;

	for (VkWriteDescriptorSet &w : writes)
		w.dstSet = set;

	vkUpdateDescriptorSets(VkWrapper::device->logicalHandle, writes.size(), writes.data(), 0, nullptr);
	return true;
}

void DescriptorWriter::writeBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset)
{
	VkDescriptorBufferInfo& buffer_info = buffer_infos.emplace_back();
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
	VkDescriptorImageInfo& image_info = image_infos.emplace_back();
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

void DescriptorWriter::clear()
{
	buffer_infos.clear();
	image_infos.clear();
	writes.clear();
}

void DescriptorWriter::updateSet(VkDescriptorSet set)
{
	for (VkWriteDescriptorSet &write : writes)
		write.dstSet = set;

	vkUpdateDescriptorSets(VkWrapper::device->logicalHandle, writes.size(), writes.data(), 0, nullptr);
}
