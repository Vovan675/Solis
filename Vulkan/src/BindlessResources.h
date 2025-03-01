#pragma once
#include "RHI/RHITexture.h"
#include "RHI/Vulkan/Descriptors.h"

class RHIBindlessResources
{
public:
	virtual void init();
	virtual void cleanup();

	void beginFrame() {}
	virtual void setTexture(uint32_t index, RHITexture *texture);
	virtual uint32_t addTexture(RHITexture *texture);
	virtual RHITexture *getTexture(uint32_t index);
	virtual void removeTexture(RHITexture *texture);
	virtual uint32_t getTextureIndex(RHITexture *texture) { return textures_indices[texture]; }

protected:
	virtual void set_invalid_texture(uint32_t index) {}

protected:
	std::shared_ptr<RHITexture> invalid_texture;

	std::unordered_map<RHITexture *, uint32_t> textures_indices;
	std::vector<int> empty_indices;
};

class VulkanBindlessResources : public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	void setTexture(uint32_t index, RHITexture *texture) override;

	void updateSets();

	VkDescriptorSetLayout getDescriptorLayout() { return bindless_layout.layout; }
	VkDescriptorSet getDescriptorSet() { return bindless_set; }
private:
	void set_invalid_texture(uint32_t index) override;

private:
	DescriptorLayout bindless_layout;
	VkDescriptorPool bindless_pool;
	VkDescriptorSet bindless_set;

	DescriptorWriter descriptor_writer;
	bool is_dirty = false;
};

class DX12BindlessResources: public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	void setTexture(uint32_t index, RHITexture *texture) override;

	void update();

	VkDescriptorSetLayout getDescriptorLayout() { return bindless_layout.layout; }
	VkDescriptorSet getDescriptorSet() { return bindless_set; }
private:
	void set_invalid_texture(uint32_t index) override;

private:
	DescriptorLayout bindless_layout;
	VkDescriptorPool bindless_pool;
	VkDescriptorSet bindless_set;

	DescriptorWriter descriptor_writer;
	bool is_dirty = false;
};
