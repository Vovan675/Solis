#include "pch.h"
#include "RHI/BindlessResources.h"
#include "Assets/AssetManager.h"
#include "RHI/RHITexture.h"
#include "RHI/DX12/DX12DynamicRHI.h"
#include "RHI/Vulkan/VulkanUtils.h"
#include "RHI/Vulkan/VulkanDynamicRHI.h"
#include <Rendering/Renderer.h>

void RHIBindlessResources::init()
{
	// By default all indices are empty
	for (int i = MAX_BINDLESS_TEXTURES - 2; i >= 0; i--)
		empty_resource_indices.push_back(i);

	// Init default samplers
	auto createSampler = [](Filter filtering, SamplerMode sampler_mode, bool use_comparison_less = false)
	{
		TextureDescription sampler;
		sampler.filtering = filtering;
		sampler.sampler_mode = sampler_mode;
		sampler.anisotropy = true;
		sampler.use_comparison_less = use_comparison_less;
		return sampler;
	};

	// Must match in shaders
	TextureDescription linearWrapSampler = createSampler(FILTER_LINEAR, SAMPLER_MODE_REPEAT);
	TextureDescription linearClampSampler = createSampler(FILTER_LINEAR, SAMPLER_MODE_CLAMP_TO_EDGE);
	TextureDescription pointWrapSampler = createSampler(FILTER_NEAREST, SAMPLER_MODE_REPEAT);
	TextureDescription pointClampSampler = createSampler(FILTER_NEAREST, SAMPLER_MODE_CLAMP_TO_EDGE);
	TextureDescription shadowWrapSampler = createSampler(FILTER_LINEAR, SAMPLER_MODE_REPEAT, true);
	TextureDescription shadowClampSampler = createSampler(FILTER_LINEAR, SAMPLER_MODE_CLAMP_TO_EDGE, true);

	addSampler(linearWrapSampler);
	addSampler(linearClampSampler);
	addSampler(pointWrapSampler);
	addSampler(pointClampSampler);
	addSampler(shadowWrapSampler);
	addSampler(shadowClampSampler);

	invalid_texture = AssetManager::getTextureAsset("assets/invalid_texture.png");
	for (int i = MAX_BINDLESS_TEXTURES - 2; i >= 0; i--)
		set_invalid_texture(i);
}

void RHIBindlessResources::cleanup()
{
	invalid_texture = nullptr;
}

void RHIBindlessResources::setTexture(uint32_t index, RHITexture *texture)
{
	if (texture == nullptr)
	{
		set_invalid_texture(index);
		return;
	}
}

uint32_t RHIBindlessResources::addTexture(RHITexture *texture)
{
	if (empty_resource_indices.size() == 0)
		CORE_CRITICAL("Bindless: not enough indices");

	// If already exists, return index
	auto it = texture_to_resource_index.find(texture);
	if (it != texture_to_resource_index.end())
		return it->second;
	uint32_t index = empty_resource_indices.back();
	empty_resource_indices.pop_back();
	setTexture(index, texture);
	return index;
}

RHITexture *RHIBindlessResources::getTexture(uint32_t index)
{
	for (auto &tex : texture_to_resource_index)
	{
		if (tex.second == index)
			return tex.first;
	}
	return nullptr;
}

void RHIBindlessResources::removeTexture(RHITexture *texture)
{
	if (texture_to_resource_index.empty())
		return;
	// If not found, return
	auto it = texture_to_resource_index.find(texture);
	if (it == texture_to_resource_index.end())
		return;

	uint32_t index = texture_to_resource_index[texture];
	texture_to_resource_index.erase(texture);

	gDynamicRHI->releaseNextFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
		set_invalid_texture(index);
	});
}



// VULKAN

static VkDescriptorPool createBindlessDescriptorPool()
{
	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorPoolSize> poolSizes{};

	poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_BINDLESS_TEXTURES});
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLER, MAX_BINDLESS_SAMPLERS * 10});

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // flag needed for bindless
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 100000;
	CHECK_ERROR(vkCreateDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
	return descriptor_pool;
}

void VulkanBindlessResources::init()
{
	// Layout
	VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{};
	extended_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	extended_info.bindingCount = 2;
	// We dont fully fill array, so we need partial bound. Also update after binding can be used
	std::array<VkDescriptorBindingFlags, 2> binding_flags =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
	};
	extended_info.pBindingFlags = binding_flags.data();

	DescriptorLayoutBuilder layout_builder;
	layout_builder.add_binding(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_BINDLESS_TEXTURES); // All textures
	layout_builder.add_binding(BINDLESS_SAMPLERS_BINDING, VK_DESCRIPTOR_TYPE_SAMPLER, MAX_BINDLESS_SAMPLERS * 10); // All samplers

	auto flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT; // For updating textures after binding
	bindless_layout = layout_builder.build(VK_SHADER_STAGE_ALL, &extended_info, flags);

	// Pool
	bindless_pool = createBindlessDescriptorPool();

	// Set
	VkDescriptorSetAllocateInfo bindless_alloc_info{};
	bindless_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	bindless_alloc_info.descriptorPool = bindless_pool;
	bindless_alloc_info.descriptorSetCount = 1;
	bindless_alloc_info.pSetLayouts = &bindless_layout.layout;

	VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{};
	count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	count_info.descriptorSetCount = 1;
	uint32_t max_binding = MAX_BINDLESS_TEXTURES - 1;
	count_info.pDescriptorCounts = &max_binding;

	bindless_alloc_info.pNext = &count_info;

	CHECK_ERROR(vkAllocateDescriptorSets(VulkanUtils::getNativeRHI()->device->logicalHandle, &bindless_alloc_info, &bindless_set));

	RHIBindlessResources::init();
}

void VulkanBindlessResources::cleanup()
{
	// Descriptor sets will implicitly free
	vkDestroyDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, bindless_pool, nullptr);
	for (auto sampler : samplers)
		gDynamicRHI->releaseGPUResource(sampler);
}

void VulkanBindlessResources::setTexture(uint32_t index, RHITexture *texture)
{
	RHIBindlessResources::setTexture(index, texture);
	if (texture == nullptr || (texture->getUsageFlags() & TEXTURE_USAGE_NO_SAMPLED))
		return;
	texture_to_resource_index[texture] = index;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	VulkanTexture *native_texture = (VulkanTexture *)texture;
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, native_texture->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
}

uint32_t VulkanBindlessResources::addSampler(const TextureDescription &description)
{
	VkFilter filter = VK_FILTER_LINEAR;
	if (description.filtering == FILTER_LINEAR)
		filter = VK_FILTER_LINEAR;
	else if (description.filtering == FILTER_NEAREST)
		filter = VK_FILTER_NEAREST;

	VkSamplerAddressMode sampler_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (description.sampler_mode == SAMPLER_MODE_REPEAT)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_EDGE)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_BORDER)
		sampler_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = sampler_mode;
	samplerInfo.addressModeV = sampler_mode;
	samplerInfo.addressModeW = sampler_mode;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	if (description.use_comparison_less)
	{
		samplerInfo.compareEnable = true;
		samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // For shadows hardware comparison
	}

	samplerInfo.anisotropyEnable = description.anisotropy;
	samplerInfo.maxAnisotropy = description.anisotropy ? VulkanUtils::getNativeRHI()->device->physicalProperties.properties.limits.maxSamplerAnisotropy : 1.0f;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = description.mip_levels;

	VkSamplerResource *sampler = new VkSamplerResource();
	CHECK_ERROR(vkCreateSampler(VulkanUtils::getNativeRHI()->device->logicalHandle, &samplerInfo, nullptr, &sampler->resource));
	samplers.push_back(sampler);

	descriptor_writer.writeImage(BINDLESS_SAMPLERS_BINDING, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, sampler->resource, VK_IMAGE_LAYOUT_UNDEFINED);
	descriptor_writer.writes.back().dstArrayElement = samplers.size() - 1;
	is_dirty = true;
	updateSets();

	return samplers.size() - 1;
}

VkSampler VulkanBindlessResources::getNativeSampler(uint32_t sampler_index)
{
	return samplers[sampler_index]->resource;
}

void VulkanBindlessResources::updateSets()
{
	if (!is_dirty)
		return;
	is_dirty = false;

	descriptor_writer.updateSet(bindless_set);
	descriptor_writer.clear();
}

void VulkanBindlessResources::set_invalid_texture(uint32_t index)
{
	if (!invalid_texture->isValid())
		return;

	if (invalid_texture == nullptr)
		return;

	VulkanTexture *native_texture = (VulkanTexture *)invalid_texture.getReference();
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, native_texture->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
}

// DX12

void DX12BindlessResources::init()
{
	RHIBindlessResources::init();
}

void DX12BindlessResources::cleanup()
{}

void DX12BindlessResources::setTexture(uint32_t index, RHITexture *texture)
{
	RHIBindlessResources::setTexture(index, texture);
	if (texture == nullptr)
		return;
	texture_to_resource_index[texture] = index;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	DX12Texture *native_texture = (DX12Texture *)texture;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());

	// Copy from staging heap, to current frame's shader visible heap
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, native_texture->shader_resource_view.getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12BindlessResources::update()
{}

uint32_t DX12BindlessResources::addSampler(const TextureDescription &description)
{
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;
	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_samplers_heap(rhi->samplers_heap->getHandle(rhi->samplers_heap->getCount()).getCpuHandle());

	// Allocate in samplers heap
	DX12Descriptor sampler_view = rhi->samplers_heap->allocate();

	D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	if (description.filtering == FILTER_LINEAR)
		filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	else if (description.filtering == FILTER_NEAREST)
		filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

	D3D12_TEXTURE_ADDRESS_MODE address_mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	if (description.sampler_mode == SAMPLER_MODE_REPEAT)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_EDGE)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	else if (description.sampler_mode == SAMPLER_MODE_CLAMP_TO_BORDER)
		address_mode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;


	D3D12_SAMPLER_DESC sampler_desc = {};
	sampler_desc.Filter = filter;
	sampler_desc.AddressU = address_mode;
	sampler_desc.AddressV = address_mode;
	sampler_desc.AddressW = address_mode;
	sampler_desc.MipLODBias = 0;
	sampler_desc.MaxAnisotropy = description.anisotropy ? 4 : 1.0f;
	sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D12_FLOAT32_MAX;
	sampler_desc.BorderColor[0] = 1.0f;
	sampler_desc.BorderColor[1] = 1.0f;
	sampler_desc.BorderColor[2] = 1.0f;
	sampler_desc.BorderColor[3] = 1.0f;

	if (description.use_comparison_less)
	{
		D3D12_FILTER filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		if (description.filtering == FILTER_LINEAR)
			filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		else if (description.filtering == FILTER_NEAREST)
			filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		sampler_desc.Filter = filter;
		sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // For shadows hardware comparison
	}

	rhi->device->CreateSampler(&sampler_desc, sampler_view.getCpuHandle());
	return sampler_view.getIndex();
}

void DX12BindlessResources::set_invalid_texture(uint32_t index)
{
	if (!invalid_texture->isValid())
		return;
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;
	DX12Texture *texture = (DX12Texture *)invalid_texture.getReference();

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());

	// Copy from staging heap, to current frame's shader visible heap
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, texture->shader_resource_view.getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
