#pragma once
#include "RHI/Texture.h"
#include "RHI/Descriptors.h"

class BindlessResources
{
public:
	static void init();
	static void cleanup();

	static const VkDescriptorSetLayout *getDescriptorLayout() { return &bindless_layout; }
	static const VkDescriptorSet *getDescriptorSet() { return &bindless_set; }

	static void setTexture(uint32_t index, Texture *texture);
	static uint32_t addTexture(Texture *texture);
	static void removeTexture(Texture *texture);
	static uint32_t getTextureIndex(Texture *texture) { return textures_indices[texture]; }
	static void updateSets();
private:
	static void set_invalid_texture(uint32_t index);

private:
	static std::shared_ptr<Texture> invalid_texture;

	static VkDescriptorSetLayout bindless_layout;
	static VkDescriptorPool bindless_pool;
	static VkDescriptorSet bindless_set;

	static DescriptorWriter descriptor_writer;
	static bool is_dirty;

	static std::unordered_map<Texture *, uint32_t> textures_indices;
	static std::vector<int> empty_indices;
};

