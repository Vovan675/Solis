#pragma once
#include "RHI/BindlessResources.h"
#include "Descriptors.h"
#include "VulkanResources.h"

class VulkanBindlessResources final: public RHIBindlessResources
{
public:
	void init() override;
	void cleanup() override;

	uint32_t addSampler(const SamplerDescription &description) override;
	VkSampler getNativeSampler(uint32_t sampler_index);

	void updateSets();

	VkDescriptorSetLayout getDescriptorLayout() { return bindless_layout.layout; }
	VkDescriptorSet getDescriptorSet() { return bindless_set; }

private:
	void write_texture(uint32_t index, RHITextureView *view) override;
	void write_buffer(uint32_t index, RHIBufferView *view) override;
	void write_acceleration_structure(uint32_t index, RHITopLevelAccelerationStructure *as) override;
	void set_invalid_texture(uint32_t index) override;
	void recreate_sampler(uint32_t index) override;

private:
	DescriptorLayout bindless_layout;
	VkDescriptorPool bindless_pool;
	VkDescriptorSet bindless_set;

	DescriptorWriter descriptor_writer;

	eastl::vector<VkSamplerResource *> samplers;
};
