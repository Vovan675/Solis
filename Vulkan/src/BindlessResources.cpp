#include "pch.h"
#include "BindlessResources.h"
#include "Assets/AssetManager.h"
#include "RHI/RHITexture.h"
#include "RHI/DX12/DX12DynamicRHI.h"
#include "RHI/Vulkan/VulkanUtils.h"
#include "RHI/Vulkan/VulkanDynamicRHI.h"
#include <Rendering/Renderer.h>

static const int MAX_BINDLESS_TEXTURES = 1000;
static const int BINDLESS_TEXTURES_BINDING = 0;

void RHIBindlessResources::init()
{
	// Load invalid texture
	TextureDescription description;
	invalid_texture = AssetManager::getTextureAsset("assets/invalid_texture.png");
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
	if (empty_indices.size() == 0)
		throw "not enough indices";

	// If already exists, return index
	auto it = textures_indices.find(texture);
	if (it != textures_indices.end())
		return it->second;
	uint32_t index = empty_indices.back();
	empty_indices.pop_back();
	setTexture(index, texture);
	return index;
}

RHITexture *RHIBindlessResources::getTexture(uint32_t index)
{
	for (auto &tex : textures_indices)
	{
		if (tex.second == index)
			return tex.first;
	}
	return nullptr;
}

void RHIBindlessResources::removeTexture(RHITexture *texture)
{
	if (textures_indices.empty())
		return;
	// If not found, return
	auto it = textures_indices.find(texture);
	if (it == textures_indices.end())
		return;

	uint32_t index = textures_indices[texture];
	textures_indices.erase(texture);
	empty_indices.push_back(index);

	set_invalid_texture(index);
}



// VULKAN

static VkDescriptorPool createBindlessDescriptorPool(uint32_t bindless_samplers_count)
{
	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorPoolSize> poolSizes{};

	if (bindless_samplers_count != 0)
		poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, bindless_samplers_count});

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // flag needed for bindless
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = bindless_samplers_count;
	CHECK_ERROR(vkCreateDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
	return descriptor_pool;
}

void VulkanBindlessResources::init()
{
	RHIBindlessResources::init();

	// Layout
	VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info{};
	extended_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	extended_info.bindingCount = 1;
	// We dont fully fill array, so we need partial bound and variable count. Also update after binding can be used
	VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
	extended_info.pBindingFlags = &binding_flags;

	DescriptorLayoutBuilder layout_builder;
	layout_builder.add_binding(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_TEXTURES); // All textures

	auto flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT; // For updating textures after binding
	bindless_layout = layout_builder.build(VK_SHADER_STAGE_ALL, &extended_info, flags);

	// Pool
	bindless_pool = createBindlessDescriptorPool(MAX_BINDLESS_TEXTURES);

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


	// By default all indices are empty
	for (int i = MAX_BINDLESS_TEXTURES - 2; i >= 0; i--)
	{
		empty_indices.push_back(i);
		set_invalid_texture(i);	
	}
}

void VulkanBindlessResources::cleanup()
{
	// Descriptor sets will implicitly free
	vkDestroyDescriptorPool(VulkanUtils::getNativeRHI()->device->logicalHandle, bindless_pool, nullptr);
}

void VulkanBindlessResources::setTexture(uint32_t index, RHITexture *texture)
{
	RHIBindlessResources::setTexture(index, texture);
	if (texture == nullptr)
		return;
	textures_indices[texture] = index;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	VulkanTexture *native_texture = (VulkanTexture *)texture;
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, native_texture->getImageView(), native_texture->getSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
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
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, native_texture->getImageView(), native_texture->getSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
	updateSets();
}

// DX12

void DX12BindlessResources::init()
{
	RHIBindlessResources::init();

	// By default all indices are empty
	for (int i = MAX_BINDLESS_TEXTURES - 2; i >= 0; i--)
	{
		empty_indices.push_back(i);
		set_invalid_texture(i);	
	}
}

void DX12BindlessResources::cleanup()
{}

void DX12BindlessResources::setTexture(uint32_t index, RHITexture *texture)
{
	RHIBindlessResources::setTexture(index, texture);
	if (texture == nullptr)
		return;
	textures_indices[texture] = index;
	CORE_INFO("Set texture {} at index {} w {} h {}", texture->getDebugName(), index, texture->getWidth(), texture->getHeight());

	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;

	DX12Texture *native_texture = (DX12Texture *)texture;

	// TODO: 
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap[i]->GetCPUDescriptorHandleForHeapStart());
		cpu_handle_srv_heap.Offset(rhi->cbv_srv_uav_heap_bindless_start + index, rhi->cbv_srv_uav_descriptor_size);

		// Copy from staging heap, to current frame's shader visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = native_texture->shader_resource_view;
		rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_samplers_heap(rhi->samplers_heap[i]->GetCPUDescriptorHandleForHeapStart());
			cpu_handle_samplers_heap.Offset(rhi->sampler_heap_bindless_start + index, rhi->sampler_descriptor_size);

			// Copy from staging heap, to current frame's shader visible heap
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = native_texture->sampler_view;
			rhi->device->CopyDescriptorsSimple(1, cpu_handle_samplers_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
	}
}

void DX12BindlessResources::update()
{}

void DX12BindlessResources::set_invalid_texture(uint32_t index)
{
	if (!invalid_texture->isValid())
		return;
	DX12DynamicRHI *rhi = (DX12DynamicRHI *)gDynamicRHI;
	DX12Texture *texture = (DX12Texture *)invalid_texture.getReference();

	// TODO: 
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_srv_heap(rhi->cbv_srv_uav_heap[i]->GetCPUDescriptorHandleForHeapStart());
		cpu_handle_srv_heap.Offset(rhi->cbv_srv_uav_heap_bindless_start + index, rhi->cbv_srv_uav_descriptor_size);

		// Copy from staging heap, to current frame's shader visible heap
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = texture->shader_resource_view;
		rhi->device->CopyDescriptorsSimple(1, cpu_handle_srv_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_handle_samplers_heap(rhi->samplers_heap[i]->GetCPUDescriptorHandleForHeapStart());
			cpu_handle_samplers_heap.Offset(rhi->sampler_heap_bindless_start + index, rhi->sampler_descriptor_size);

			// Copy from staging heap, to current frame's shader visible heap
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_staging_heap = texture->sampler_view;
			rhi->device->CopyDescriptorsSimple(1, cpu_handle_samplers_heap, cpu_handle_staging_heap, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}
	}
}
