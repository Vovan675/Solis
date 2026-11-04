#include "pch.h"
#include "RHI/BindlessResources.h"
#include "Assets/AssetManager.h"

void RHIBindlessResources::init()
{
	// By default all indices are empty (zero bindless index is invalid)
	for (int i = MAX_BINDLESS_RESOURCES - 2; i > 0; i--)
		empty_resource_indices.push_back(i);

	// Init default samplers
	auto createSampler = [](Filter filter, SamplerAddressMode address_mode, bool use_comparison_less = false)
	{
		SamplerDescription sampler;
		sampler.filter = filter;
		sampler.address_mode = address_mode;
		sampler.anisotropy = true;
		sampler.use_comparison_less = use_comparison_less;
		return sampler;
	};

	// Must match in shaders
	SamplerDescription linearWrapSampler = createSampler(Filter::LINEAR, SamplerAddressMode::REPEAT);
	SamplerDescription linearClampSampler = createSampler(Filter::LINEAR, SamplerAddressMode::CLAMP_TO_EDGE);
	SamplerDescription pointWrapSampler = createSampler(Filter::NEAREST, SamplerAddressMode::REPEAT);
	SamplerDescription pointClampSampler = createSampler(Filter::NEAREST, SamplerAddressMode::CLAMP_TO_EDGE);
	SamplerDescription shadowWrapSampler = createSampler(Filter::LINEAR, SamplerAddressMode::REPEAT, true);
	SamplerDescription shadowClampSampler = createSampler(Filter::LINEAR, SamplerAddressMode::CLAMP_TO_EDGE, true);

	addSampler(linearWrapSampler);
	addSampler(linearClampSampler);
	addSampler(pointWrapSampler);
	addSampler(pointClampSampler);
	addSampler(shadowWrapSampler);
	addSampler(shadowClampSampler);

	invalid_texture = AssetManager::getTextureAsset("assets/textures/core/invalid_texture.png");
	for (int i = MAX_BINDLESS_RESOURCES - 2; i >= 0; i--)
		set_invalid_texture(i);
}

void RHIBindlessResources::setSamplersMipBias(float bias)
{
	for (uint32_t i = 0; i < sampler_descriptions.size(); i++)
	{
		SamplerDescription &description = sampler_descriptions[i];
		if (description.mip_bias == bias || description.filter != Filter::LINEAR || description.use_comparison_less)
			continue;
		description.mip_bias = bias;
		recreate_sampler(i);
	}
}

uint32_t RHIBindlessResources::addTexture(RHITextureView *view)
{
	if (empty_resource_indices.size() == 0)
		CORE_CRITICAL("Bindless: not enough indices");

	// If already exists, return index
	auto it = texture_view_to_resource_index.find(view);
	if (it != texture_view_to_resource_index.end())
		return it->second;
	uint32_t index = empty_resource_indices.back();
	empty_resource_indices.pop_back();
	texture_view_to_resource_index[view] = index;
	write_texture(index, view);
	return index;
}

RHITexture *RHIBindlessResources::getTexture(uint32_t index)
{
	for (auto &tex : texture_view_to_resource_index)
	{
		if (tex.second == index)
			return tex.first->getDescription().texture;
	}
	return nullptr;
}

void RHIBindlessResources::removeTexture(RHITextureView *view)
{
	if (texture_view_to_resource_index.empty())
		return;
	// If not found, return
	auto it = texture_view_to_resource_index.find(view);
	if (it == texture_view_to_resource_index.end())
		return;

	uint32_t index = texture_view_to_resource_index[view];
	texture_view_to_resource_index.erase(view);

	gDynamicRHI->runAfterGPUFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
		set_invalid_texture(index);
	});
}

uint32_t RHIBindlessResources::addBuffer(RHIBufferView *view)
{
	if (empty_resource_indices.size() == 0)
		CORE_CRITICAL("Bindless: not enough indices");

	// If already exists, return index
	auto it = buffer_view_to_resource_index.find(view);
	if (it != buffer_view_to_resource_index.end())
		return it->second;
	uint32_t index = empty_resource_indices.back();
	empty_resource_indices.pop_back();
	buffer_view_to_resource_index[view] = index;

	RHIBuffer *buffer = view->getDescription().buffer;
	ENGINE_ASSERT_MSG(hasAnyFlags(buffer->getUsageFlags(), BufferUsage::SHADER_RESOURCE | BufferUsage::UNORDERED_ACCESS), "Only Bindless Storage Buffers Supported");
	write_buffer(index, view);
	return index;
}

void RHIBindlessResources::removeBuffer(RHIBufferView *view)
{
	if (buffer_view_to_resource_index.empty())
		return;
	// If not found, return
	auto it = buffer_view_to_resource_index.find(view);
	if (it == buffer_view_to_resource_index.end())
		return;

	uint32_t index = buffer_view_to_resource_index[view];
	buffer_view_to_resource_index.erase(view);

	gDynamicRHI->runAfterGPUFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
	});
}

uint32_t RHIBindlessResources::addAccelerationStructure(RHITopLevelAccelerationStructure *as)
{
	if (empty_resource_indices.size() == 0)
		CORE_CRITICAL("Bindless: not enough indices");

	// If already exists, return index
	auto it = acceleration_structure_to_resource_index.find(as);
	if (it != acceleration_structure_to_resource_index.end())
		return it->second;
	uint32_t index = empty_resource_indices.back();
	empty_resource_indices.pop_back();
	acceleration_structure_to_resource_index[as] = index;

	write_acceleration_structure(index, as);
	return index;
}

void RHIBindlessResources::removeAccelerationStructure(RHITopLevelAccelerationStructure *as)
{
	if (acceleration_structure_to_resource_index.empty())
		return;
	// If not found, return
	auto it = acceleration_structure_to_resource_index.find(as);
	if (it == acceleration_structure_to_resource_index.end())
		return;

	uint32_t index = acceleration_structure_to_resource_index[as];
	acceleration_structure_to_resource_index.erase(as);

	gDynamicRHI->runAfterGPUFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
	});
}
