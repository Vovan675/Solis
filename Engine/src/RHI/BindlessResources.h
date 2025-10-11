#pragma once
#include "RHI/RHIDefinitions.h"
#include "RHI/RHITexture.h"
#include "RHI/Vulkan/Descriptors.h"
#include "RHI/Vulkan/VulkanResources.h"

static const int MAX_BINDLESS_TEXTURES = 4096;
static const int MAX_BINDLESS_SAMPLERS = 2048;

static const int BINDLESS_TEXTURES_BINDING = 0;
static const int BINDLESS_TEXTURES_SET = 1;

static const int BINDLESS_SAMPLERS_BINDING = 1;
static const int BINDLESS_SAMPLERS_SET = 1;

class RHIBindlessResources
{
public:
	virtual void init();
	virtual void cleanup();

	void finishFrame() {}
	virtual void setTexture(uint32_t index, RHITexture *texture);
	virtual uint32_t addTexture(RHITexture *texture);
	virtual RHITexture *getTexture(uint32_t index);
	virtual void removeTexture(RHITexture *texture);
	virtual uint32_t getTextureIndex(RHITexture *texture) { return texture_to_resource_index[texture]; }

	virtual uint32_t addSampler(const TextureDescription &description) { return 0; }
protected:
	virtual void set_invalid_texture(uint32_t index) {}

protected:
	RHITextureRef invalid_texture;

	eastl::unordered_map<RHITexture *, uint32_t> texture_to_resource_index;
	eastl::vector<int> empty_resource_indices;
	
	eastl::unordered_map<RHITexture *, uint32_t> texture_to_sampler_index;
};

class VulkanBindlessResources final: public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	void setTexture(uint32_t index, RHITexture *texture) override;
	uint32_t addSampler(const TextureDescription &description) override;
	VkSampler getNativeSampler(uint32_t sampler_index);

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

	eastl::vector<VkSamplerResource *> samplers;
};

class DX12BindlessResources final: public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	void setTexture(uint32_t index, RHITexture *texture) override;

	void update();

	uint32_t addSampler(const TextureDescription &description) override;
private:
	void set_invalid_texture(uint32_t index) override;

private:
};
