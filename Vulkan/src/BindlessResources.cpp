#include "pch.h"
#include "BindlessResources.h"
#include "VkWrapper.h"

VkDescriptorSetLayout BindlessResources::bindless_layout;
VkDescriptorPool BindlessResources::bindless_pool;
VkDescriptorSet BindlessResources::bindless_set;

DescriptorWriter BindlessResources::descriptor_writer;
std::vector<VkDescriptorImageInfo> BindlessResources::textures_writes;
bool BindlessResources::is_dirty = false;

static const int MAX_BINDLESS_TEXTURES = 1024;
static const int BINDLESS_TEXTURES_BINDING = 0;

void BindlessResources::init()
{
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
	bindless_pool = VkWrapper::createBindlessDescriptorPool(MAX_BINDLESS_TEXTURES);

	// Set
	VkDescriptorSetAllocateInfo bindless_alloc_info{};
	bindless_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	bindless_alloc_info.descriptorPool = bindless_pool;
	bindless_alloc_info.descriptorSetCount = 1;
	bindless_alloc_info.pSetLayouts = &bindless_layout;

	VkDescriptorSetVariableDescriptorCountAllocateInfo count_info{};
	count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	count_info.descriptorSetCount = 1;
	uint32_t max_binding = MAX_BINDLESS_TEXTURES - 1;
	count_info.pDescriptorCounts = &max_binding;

	bindless_alloc_info.pNext = &count_info;

	CHECK_ERROR(vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &bindless_alloc_info, &bindless_set));
}

void BindlessResources::cleanup()
{
	// Descriptor sets will implicitly free
	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, bindless_pool, nullptr);
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, bindless_layout, nullptr);
}

void BindlessResources::setTexture(uint32_t index, Texture *texture)
{
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture->imageView, texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
}

void BindlessResources::updateSets()
{
	if (!is_dirty)
		return;
	is_dirty = false;

	descriptor_writer.updateSet(bindless_set);
	descriptor_writer.clear();
}
