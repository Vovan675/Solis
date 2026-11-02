#include "pch.h"
#include "VulkanBindlessResources.h"
#include "VulkanDynamicRHI.h"
#include "VulkanUtils.h"
#include "Core/Variables.h"

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
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
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

void VulkanBindlessResources::write_texture(uint32_t index, RHITextureView *view)
{
	VulkanTextureView *native_view = (VulkanTextureView *)view;
	if (view->getViewType() == TextureViewType::SHADER_RESOURCE)
	{
		descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, native_view->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	} else if (view->getViewType() == TextureViewType::UNORDERED_ACCESS)
	{
		descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, native_view->getImageView(), nullptr, VK_IMAGE_LAYOUT_GENERAL);
	}
	descriptor_writer.writes.back().dstArrayElement = index;
}

void VulkanBindlessResources::write_buffer(uint32_t index, RHIBufferView *view)
{
	VulkanBuffer *native_buffer = (VulkanBuffer *)view->getDescription().buffer;
	descriptor_writer.writeBuffer(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, native_buffer->getBuffer(), native_buffer->getSize());
	descriptor_writer.writes.back().dstArrayElement = index;
}

void VulkanBindlessResources::write_acceleration_structure(uint32_t index, RHITopLevelAccelerationStructure *as)
{
	VulkanTopLevelAccelerationStructure *native_as = (VulkanTopLevelAccelerationStructure *)as;
	descriptor_writer.writeAccelerationStructure(BINDLESS_RESOURCES_BINDING, &native_as->handle->resource);
	descriptor_writer.writes.back().dstArrayElement = index;
}

void VulkanBindlessResources::set_invalid_texture(uint32_t index)
{
	if (invalid_texture == nullptr || !invalid_texture->isValid())
		return;

	VulkanTexture *native_texture = (VulkanTexture *)invalid_texture.getReference();
	VulkanTextureView *view = (VulkanTextureView *)native_texture->getShaderResourceView();
	descriptor_writer.writeImage(BINDLESS_RESOURCES_BINDING, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, view->getImageView(), nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
}

uint32_t VulkanBindlessResources::addSampler(const SamplerDescription &description)
{
	VkSamplerResource *sampler = new VkSamplerResource();
	sampler->resource = VK_NULL_HANDLE;
	samplers.push_back(sampler);
	sampler_descriptions.push_back(description);

	uint32_t index = samplers.size() - 1;
	recreate_sampler(index);
	return index;
}

void VulkanBindlessResources::recreate_sampler(uint32_t index)
{
	const SamplerDescription &description = sampler_descriptions[index];

	VkFilter filter = VK_FILTER_LINEAR;
	if (description.filter == Filter::LINEAR)
		filter = VK_FILTER_LINEAR;
	else if (description.filter == Filter::NEAREST)
		filter = VK_FILTER_NEAREST;

	VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	if (description.address_mode == SamplerAddressMode::REPEAT)
		address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	else if (description.address_mode == SamplerAddressMode::CLAMP_TO_EDGE)
		address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	else if (description.address_mode == SamplerAddressMode::CLAMP_TO_BORDER)
		address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.mipmapMode = description.filter == Filter::LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = address_mode;
	samplerInfo.addressModeV = address_mode;
	samplerInfo.addressModeW = address_mode;
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
	samplerInfo.mipLodBias = description.mip_bias;
	samplerInfo.minLod = 0;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	VkSamplerResource *sampler = samplers[index];
	if (sampler->resource != VK_NULL_HANDLE)
		sampler->release();
	CHECK_ERROR(vkCreateSampler(VulkanUtils::getNativeRHI()->device->logicalHandle, &samplerInfo, nullptr, &sampler->resource));

	descriptor_writer.writeImage(BINDLESS_SAMPLERS_BINDING, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, sampler->resource, VK_IMAGE_LAYOUT_UNDEFINED);
	descriptor_writer.writes.back().dstArrayElement = index;
}

VkSampler VulkanBindlessResources::getNativeSampler(uint32_t sampler_index)
{
	return samplers[sampler_index]->resource;
}

void VulkanBindlessResources::updateSets()
{
	descriptor_writer.updateSet(bindless_set);
	descriptor_writer.clear();
}
