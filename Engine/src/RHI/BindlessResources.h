#pragma once
#include "RHI/RHIDefinitions.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"
#include "RHI/Vulkan/Descriptors.h"
#include "RHI/Vulkan/VulkanResources.h"

static const int MAX_BINDLESS_RESOURCES = 4096;
static const int MAX_BINDLESS_SAMPLERS = 2048;

static const int BINDLESS_RESOURCES_BINDING = 0;
static const int BINDLESS_RESOURCES_SET = 1;

static const int BINDLESS_SAMPLERS_BINDING = 1;
static const int BINDLESS_SAMPLERS_SET = 1;

// Bindless Resources:
// Storage Buffers (SRV, UAV)
// Textures (SRV, UAV)
// Samplers
// Acceleration Structures

class RHIBindlessResources
{
public:
	virtual void init();
	virtual void cleanup();

	void finishFrame() {}
	virtual void setTexture(uint32_t index, RHITextureView *view);
	virtual uint32_t addTexture(RHITextureView *view);
	virtual RHITexture *getTexture(uint32_t index);
	virtual void removeTexture(RHITextureView *view);

	virtual uint32_t addBuffer(RHIBufferView *view);
	virtual void removeBuffer(RHIBufferView *view);
	virtual void setBuffer(uint32_t index, RHIBufferView *view);

	virtual uint32_t addAccelerationStructure(RHITopLevelAccelerationStructure *as);
	virtual void removeAccelerationStructure(RHITopLevelAccelerationStructure *as);
	virtual void setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as);

	virtual uint32_t addSampler(const TextureDescription &description) { return 0; }
protected:
	virtual void set_invalid_texture(uint32_t index) {}

protected:
	RHITextureRef invalid_texture;

	eastl::unordered_map<RHITextureView *, uint32_t> texture_view_to_resource_index;
	eastl::unordered_map<RHIBufferView *, uint32_t> buffer_to_resource_index;
	eastl::unordered_map<RHITopLevelAccelerationStructure *, uint32_t> acceleration_structure_to_resource_index;
	eastl::vector<int> empty_resource_indices;
	
	eastl::unordered_map<RHITexture *, uint32_t> texture_to_sampler_index;
};

class VulkanBindlessResources final: public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	void setTexture(uint32_t index, RHITextureView *view) override;
	void setBuffer(uint32_t index, RHIBufferView *view) override;
	void setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as) override;

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

	void setTexture(uint32_t index, RHITextureView *view) override;
	void setBuffer(uint32_t index, RHIBufferView *view) override;
	void setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as) override;

	void update();

	uint32_t addSampler(const TextureDescription &description) override;
private:
	void set_invalid_texture(uint32_t index) override;

private:
};
