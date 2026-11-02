#pragma once
#include "RHI/BindlessResources.h"
#include "DX12DescriptorHeap.h"

class DX12BindlessResources final: public RHIBindlessResources
{
public:
	uint32_t addSampler(const SamplerDescription &description) override;

private:
	void write_texture(uint32_t index, RHITextureView *view) override;
	void write_buffer(uint32_t index, RHIBufferView *view) override;
	void write_acceleration_structure(uint32_t index, RHITopLevelAccelerationStructure *as) override;
	void set_invalid_texture(uint32_t index) override;
	void recreate_sampler(uint32_t index) override;

	void copy_descriptor(uint32_t index, const DX12Descriptor &descriptor);

	eastl::vector<uint32_t> sampler_heap_indices;
};
