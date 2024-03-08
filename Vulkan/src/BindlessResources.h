#pragma once
#include "Texture.h"
#include "Descriptors.h"

class BindlessResources
{
public:
	static void init();
	static void cleanup();

	static const VkDescriptorSetLayout *getDescriptorLayout() { return &bindless_layout; }
	static const VkDescriptorSet *getDescriptorSet() { return &bindless_set; }

	static void setTexture(uint32_t index, Texture* texture);
	static void updateSets();
private:
	static VkDescriptorSetLayout bindless_layout;
	static VkDescriptorPool bindless_pool;
	static VkDescriptorSet bindless_set;

	static DescriptorWriter descriptor_writer;
	static std::vector<VkDescriptorImageInfo> textures_writes;
	static bool is_dirty;
};

