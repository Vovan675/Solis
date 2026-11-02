#pragma once
#include "Device.h"
#include <EASTL/deque.h>

enum class DescriptorStage : uint32_t
{
	// Default pipeline
	VERTEX = 2 << 0,
	FRAGMENT = 2 << 1,
	// Compute pipeline
	COMPUTE = 2 << 2,
	// Ray tracing pipeline
	RAY_GENERATION = 2 << 3,
	MISS = 2 << 4,
	CLOSEST_HIT = 2 << 5,
};
ALLOW_ENUM_BITS(DescriptorStage)

enum class DescriptorType
{
	PUSH_CONSTANT,
	UNIFORM_BUFFER,
	STORAGE_BUFFER,
	SAMPLER,
	SAMPLED_IMAGE,
	COMBINED_IMAGE,
	STORAGE_IMAGE,
	ACCELERATION_STRUCTURE,
};

struct Descriptor
{
	Descriptor(DescriptorType type, unsigned int set, unsigned int binding, size_t size, DescriptorStage stage, eastl::string name = "")
		: type(type), set(set), binding(binding), size(size), stage(stage), name(name)
	{

	}

	DescriptorType type;
	unsigned int set;
	unsigned int binding;
	size_t size;
	DescriptorStage stage;
	eastl::string name;

	unsigned int first_member_offset = 0;
};

struct DescriptorLayout
{
    VkDescriptorSetLayout layout = nullptr;
    size_t hash;
};

struct DescriptorLayoutBuilder
{
    eastl::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type, uint32_t count = 1);
    void clear();
    static void clearAllCaches();

    DescriptorLayout build(VkShaderStageFlags stages, const void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
private:
    static eastl::unordered_map<size_t, DescriptorLayout> cached_descriptor_layouts;
};

// Manages allocation of descriptor sets
class DescriptorAllocator
{
public:
    eastl::vector<VkDescriptorPoolSize> pool_sizes = {
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
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 500 },
	};

	void cleanup();
    void resetPools();

    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
    VkDescriptorPool get_pool();
	VkDescriptorPool create_pool();
private:
    eastl::vector<VkDescriptorPool> free_pools;
    eastl::vector<VkDescriptorPool> used_pools;
};

struct DescriptorWriter
{
    eastl::deque<VkDescriptorBufferInfo> buffer_infos;
    eastl::deque<VkDescriptorImageInfo> image_infos;
    eastl::deque<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_infos;
    eastl::vector<VkWriteDescriptorSet> writes;

    void writeBuffer(uint32_t binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset = 0);
    void writeImage(uint32_t binding, VkDescriptorType type, VkImageView image, VkSampler sampler, VkImageLayout image_layout);

    void writeAccelerationStructure(uint32_t binding, VkAccelerationStructureKHR *acceleration_structure);


    void clear();
    void updateSet(VkDescriptorSet set);
};
