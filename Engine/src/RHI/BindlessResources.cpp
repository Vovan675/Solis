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
	// By default all indices are empty (zero bindless index is invalid)
	for (int i = MAX_BINDLESS_RESOURCES - 2; i > 0; i--)
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
	for (int i = MAX_BINDLESS_RESOURCES - 2; i >= 0; i--)
		set_invalid_texture(i);
}

void RHIBindlessResources::cleanup()
{
	invalid_texture = nullptr;
}

void RHIBindlessResources::setTexture(uint32_t index, RHITextureView *view)
{
	if (view == nullptr)
	{
		set_invalid_texture(index);
		return;
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
	setTexture(index, view);
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

	gDynamicRHI->releaseNextFrame([this, index]()
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
	auto it = buffer_to_resource_index.find(view);
	if (it != buffer_to_resource_index.end())
		return it->second;
	uint32_t index = empty_resource_indices.back();
	empty_resource_indices.pop_back();
	setBuffer(index, view);
	return index;
}

void RHIBindlessResources::removeBuffer(RHIBufferView *view)
{
	if (buffer_to_resource_index.empty())
		return;
	// If not found, return
	auto it = buffer_to_resource_index.find(view);
	if (it == buffer_to_resource_index.end())
		return;

	uint32_t index = buffer_to_resource_index[view];
	buffer_to_resource_index.erase(view);

	gDynamicRHI->releaseNextFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
	});
}

void RHIBindlessResources::setBuffer(uint32_t index, RHIBufferView *view)
{
	ENGINE_ASSERT_MSG(hasAnyFlags(view->getDescription().buffer->getUsage(), BufferUsage::SHADER_READ_BUFFER | BufferUsage::SHADER_WRITE_BUFFER), "Only Bindless Storage Buffers Supported");
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
	setAccelerationStructure(index, as);
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

	gDynamicRHI->releaseNextFrame([this, index]()
	{
		empty_resource_indices.push_back(index);
	});
}

void RHIBindlessResources::setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as)
{}


// VULKAN

static VkDescriptorPool createBindlessDescriptorPool()
{
	VkDescriptorPool descriptor_pool;
	eastl::vector<VkDescriptorPoolSize> poolSizes{};

	poolSizes.push_back({VK_DESCRIPTOR_TYPE_MUTABLE_EXT, MAX_BINDLESS_RESOURCES});
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_BINDLESS_RESOURCES});
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_BINDLESS_RESOURCES});
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_BINDLESS_RESOURCES});
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
	eastl::array<VkDescriptorBindingFlags, 2> binding_flags =
	{
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
	};
	extended_info.pBindingFlags = binding_flags.data();
	
	eastl::fixed_vector<VkDescriptorType, 16> mutable_descriptor_types
	{
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	};

	if (engine_ray_tracing)
		mutable_descriptor_types.push_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR);

	VkMutableDescriptorTypeListEXT mutable_descriptor_type_list{};
	mutable_descriptor_type_list.descriptorTypeCount = mutable_descriptor_types.size();
	mutable_descriptor_type_list.pDescriptorTypes = mutable_descriptor_types.data();

	VkMutableDescriptorTypeCreateInfoEXT mutable_desc_info{};
	mutable_desc_info.sType = VK_STRUCTURE_TYPE_MUTABLE_DESCRIPTOR_TYPE_CREATE_INFO_EXT;
	mutable_desc_info.mutableDescriptorTypeListCount = 1;
	mutable_desc_info.pMutableDescriptorTypeLists = &mutable_descriptor_type_list;
	extended_info.pNext = &mutable_desc_info;

	DescriptorLayoutBuilder layout_builder;
	layout_builder.add_binding(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_MUTABLE_EXT, MAX_BINDLESS_RESOURCES); // All resources (textures + storage buffers)
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
	uint32_t max_binding = MAX_BINDLESS_RESOURCES - 1;
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

void VulkanBindlessResources::setTexture(uint32_t index, RHITextureView *view)
{
	RHIBindlessResources::setTexture(index, view);
	if (view == nullptr)
		return;
	texture_view_to_resource_index[view] = index;

	RHITexture *texture = view->getDescription().texture;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	VulkanTexture *native_texture = (VulkanTexture *)texture;
	VulkanTextureView *native_view = (VulkanTextureView *)view;
	if (view->getViewType() == TextureViewType::SHADER_RESOURCE)
	{
		descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, native_view->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	} else if (view->getViewType() == TextureViewType::SHADER_RESOURCE_STORAGE)
	{
		descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, native_view->getImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL);
	}
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
}

void VulkanBindlessResources::setBuffer(uint32_t index, RHIBufferView *view)
{
	RHIBindlessResources::setBuffer(index, view);
	if (view == nullptr)
	{
		empty_resource_indices.push_back(index);
		return;
	}
	buffer_to_resource_index[view] = index;
	RHIBuffer *buffer = view->getDescription().buffer;
	CORE_INFO("Set buffer at index {} size {}", index, buffer->getSize());

	VulkanBuffer *native_buffer = (VulkanBuffer *)buffer;
	descriptor_writer.writeBuffer(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, native_buffer->getBuffer(), native_buffer->getSize());
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
}

void VulkanBindlessResources::setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as)
{
	RHIBindlessResources::setAccelerationStructure(index, as);
	if (as == nullptr)
	{
		empty_resource_indices.push_back(index);
		return;
	}
	acceleration_structure_to_resource_index[as] = index;

	VulkanTopLevelAccelerationStructure *native_as = (VulkanTopLevelAccelerationStructure *)as;
	descriptor_writer.writeAccelerationStructure(BINDLESS_RESOURCES_BINDING, &native_as->handle);
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
	samplerInfo.mipmapMode = description.filtering == FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
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
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

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
	VulkanTextureView *view = (VulkanTextureView *)native_texture->getShaderResourceView();
	descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, view->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

void DX12BindlessResources::setTexture(uint32_t index, RHITextureView *view)
{
	RHIBindlessResources::setTexture(index, view);
	if (view == nullptr)
		return;
	texture_view_to_resource_index[view] = index;

	RHITexture *texture = view->getDescription().texture;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	DX12Texture *native_texture = (DX12Texture *)texture;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());

	// Copy from staging heap, to current frame's shader visible heap
	DX12TextureView *native_view = (DX12TextureView *)view;
	if (view->getViewType() == TextureViewType::SHADER_RESOURCE)
	{
		rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, native_view->getDescriptor().getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	} else if (view->getViewType() == TextureViewType::SHADER_RESOURCE_STORAGE)
	{
		rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, native_view->getDescriptor().getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

void DX12BindlessResources::setBuffer(uint32_t index, RHIBufferView *view)
{
	RHIBindlessResources::setBuffer(index, view);
	if (view == nullptr)
	{
		empty_resource_indices.push_back(index);
		return;
	}
	buffer_to_resource_index[view] = index;
	RHIBuffer *buffer = view->getDescription().buffer;
	CORE_INFO("Set buffer at index {} size {}", index, buffer->getSize());

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	DX12Buffer *native_buffer = (DX12Buffer *)buffer;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());

	// Copy from staging heap, to current frame's shader visible heap
	DX12BufferView *native_view = (DX12BufferView *)view;
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, native_view->getDescriptor().getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void DX12BindlessResources::setAccelerationStructure(uint32_t index, RHITopLevelAccelerationStructure *as)
{
	RHIBindlessResources::setAccelerationStructure(index, as);
	if (as == nullptr)
	{
		empty_resource_indices.push_back(index);
		return;
	}
	acceleration_structure_to_resource_index[as] = index;

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap->getHandle(index).getCpuHandle());

	// Copy from staging heap, to current frame's shader visible heap
	DX12TopLevelAccelerationStructure *native_as = (DX12TopLevelAccelerationStructure *)as;
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, native_as->shader_resource_view.getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
	sampler_desc.MaxAnisotropy = description.anisotropy ? 16 : 1.0f;
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
		sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // For shadows - standard approach (regular Z)
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
	DX12TextureView *view = (DX12TextureView *)texture->getShaderResourceView();
	rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, view->getDescriptor().getCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}
