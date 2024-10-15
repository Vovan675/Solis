#pragma once
#include "Device.h"
#include <deque>

struct DescriptorLayout
{
    VkDescriptorSetLayout layout = nullptr;
    size_t hash;
};

struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type, uint32_t count = 1);
    void clear();
    static void clearAllCaches();

    DescriptorLayout build(VkShaderStageFlags stages, const void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
private:
    static std::unordered_map<size_t, DescriptorLayout> cached_descriptor_layouts;
};

// Manages allocation of descriptor sets
class DescriptorAllocator
{
public:
	const std::vector<VkDescriptorPoolSize> POOL_SIZES = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 500 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 500 }
	};

	void cleanup();
    void resetPools();

    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
    VkDescriptorPool get_pool();
	VkDescriptorPool create_pool();
private:
    std::vector<VkDescriptorPool> free_pools;
    std::vector<VkDescriptorPool> used_pools;
};

struct DescriptorWriter
{
    std::deque<VkDescriptorBufferInfo> buffer_infos;
    std::deque<VkDescriptorImageInfo> image_infos;
    std::deque<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_infos;
    std::vector<VkWriteDescriptorSet> writes;

    void writeBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    void writeImage(uint32_t binding, VkDescriptorType type, VkImageView image, VkSampler sampler, VkImageLayout image_layout);

    void writeAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR *acceleration_structure);


    void clear();
    void updateSet(VkDescriptorSet set);
};
