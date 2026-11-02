#pragma once
#include "RHI/RHIDefinitions.h"
#include "RHI/RHITexture.h"
#include "RHI/RHIBuffer.h"

static const int MAX_BINDLESS_RESOURCES = 65536;
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
	virtual void cleanup() {}

	uint32_t addTexture(RHITextureView *view);
	RHITexture *getTexture(uint32_t index);
	void removeTexture(RHITextureView *view);

	uint32_t addBuffer(RHIBufferView *view);
	void removeBuffer(RHIBufferView *view);

	uint32_t addAccelerationStructure(RHITopLevelAccelerationStructure *as);
	void removeAccelerationStructure(RHITopLevelAccelerationStructure *as);

	virtual uint32_t addSampler(const SamplerDescription &description) = 0;
	void setSamplersMipBias(float bias);

protected:
	virtual void write_texture(uint32_t index, RHITextureView *view) = 0;
	virtual void write_buffer(uint32_t index, RHIBufferView *view) = 0;
	virtual void write_acceleration_structure(uint32_t index, RHITopLevelAccelerationStructure *as) = 0;
	virtual void set_invalid_texture(uint32_t index) = 0;
	virtual void recreate_sampler(uint32_t index) = 0;

protected:
	RHITextureRef invalid_texture;

	eastl::vector<SamplerDescription> sampler_descriptions;

	eastl::unordered_map<RHITextureView *, uint32_t> texture_view_to_resource_index;
	eastl::unordered_map<RHIBufferView *, uint32_t> buffer_view_to_resource_index;
	eastl::unordered_map<RHITopLevelAccelerationStructure *, uint32_t> acceleration_structure_to_resource_index;
	eastl::vector<int> empty_resource_indices;
};
