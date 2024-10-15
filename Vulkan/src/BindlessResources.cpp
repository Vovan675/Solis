#include "pch.h"
#include "BindlessResources.h"
#include "RHI/VkWrapper.h"
#include "Assets/AssetManager.h"

std::shared_ptr<Texture> BindlessResources::invalid_texture;

DescriptorLayout BindlessResources::bindless_layout;
VkDescriptorPool BindlessResources::bindless_pool;
VkDescriptorSet BindlessResources::bindless_set;

DescriptorWriter BindlessResources::descriptor_writer;
bool BindlessResources::is_dirty = false;

std::unordered_map<Texture *, uint32_t> BindlessResources::textures_indices;
std::vector<int> BindlessResources::empty_indices;


static const int MAX_BINDLESS_TEXTURES = 10000;
static const int BINDLESS_TEXTURES_BINDING = 0;

void BindlessResources::init()
{
	// Load invalid texture
	TextureDescription description;
	invalid_texture = AssetManager::getTextureAsset("assets/invalid_texture.png");

	// By default all indices are empty
	for (int i = MAX_BINDLESS_TEXTURES - 2; i >= 0; i--)
	{
		empty_indices.push_back(i);
		set_invalid_texture(i);	
	}

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
	bindless_alloc_info.pSetLayouts = &bindless_layout.layout;

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
	invalid_texture = nullptr;
	// Descriptor sets will implicitly free
	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, bindless_pool, nullptr);
}

void BindlessResources::setTexture(uint32_t index, Texture *texture)
{
	if (texture == nullptr)
	{
		set_invalid_texture(index);
		return;
	}

	textures_indices[texture] = index;
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture->getImageView(), texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
}

uint32_t BindlessResources::addTexture(Texture *texture)
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

Texture *BindlessResources::getTexture(uint32_t index)
{
	for (auto &tex : textures_indices)
	{
		if (tex.second == index)
			return tex.first;
	}
	return nullptr;
}

void BindlessResources::removeTexture(Texture *texture)
{
	// If not found, return
	auto it = textures_indices.find(texture);
	if (it == textures_indices.end())
		return;

	uint32_t index = textures_indices[texture];
	textures_indices.erase(texture);
	empty_indices.push_back(index);

	set_invalid_texture(index);
}

void BindlessResources::updateSets()
{
	if (!is_dirty)
		return;
	is_dirty = false;

	descriptor_writer.updateSet(bindless_set);
	descriptor_writer.clear();
}

void BindlessResources::set_invalid_texture(uint32_t index)
{
	if (invalid_texture == nullptr)
		return;
	descriptor_writer.writeImage(BINDLESS_TEXTURES_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, invalid_texture->getImageView(), invalid_texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	descriptor_writer.writes.back().dstArrayElement = index;
	is_dirty = true;
}

