#include "pch.h"
#include "Renderer.h"
#include "BindlessResources.h"

RendererDebugInfo Renderer::debug_info = {};
std::array<std::shared_ptr<Texture>, RENDER_TARGET_TEXTURES_COUNT> Renderer::screen_resources;
std::unordered_map<size_t, std::array<Renderer::PerFrameDescriptor, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptors;
std::unordered_map<size_t, std::array<size_t, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptors_offset;
std::unordered_map<size_t, std::array<Renderer::PerFrameDescriptorBinding, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptor_bindings;

void Renderer::init()
{
	recreateScreenResources();
}

void Renderer::recreateScreenResources()
{
	TextureDescription description;
	description.width = VkWrapper::swapchain->swap_extent.width;
	description.height = VkWrapper::swapchain->swap_extent.height;
	description.mipLevels = 1;
	description.numSamples = VK_SAMPLE_COUNT_1_BIT;

	///////////
	// GBuffer
	///////////

	auto swapchain_format = VkWrapper::swapchain->surface_format.format;

	auto create_screen_texture = [&description](RENDER_TARGETS rt, VkFormat format, VkImageAspectFlags aspect_flags, VkImageUsageFlags usage_flags, bool anisotropy = false, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		description.imageFormat = format;
		description.imageAspectFlags = aspect_flags;
		description.imageUsageFlags = usage_flags;
		description.sampler_address_mode = sampler_address_mode;
		description.anisotropy = anisotropy;
		screen_resources[rt] = std::make_shared<Texture>(description);
		screen_resources[rt]->fill();

		BindlessResources::addTexture(screen_resources[rt].get());
	};

	// Albedo
	create_screen_texture(RENDER_TARGET_GBUFFER_ALBEDO, swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);


	// Normal
	create_screen_texture(RENDER_TARGET_GBUFFER_NORMAL, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	// Depth-Stencil
	create_screen_texture(RENDER_TARGET_GBUFFER_DEPTH_STENCIL, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	// Position
	create_screen_texture(RENDER_TARGET_GBUFFER_POSITION, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	// Shading
	create_screen_texture(RENDER_TARGET_GBUFFER_SHADING, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	///////////
	// Lighting
	///////////

	create_screen_texture(RENDER_TARGET_LIGHTING_DIFFUSE, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	create_screen_texture(RENDER_TARGET_LIGHTING_SPECULAR, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);


	create_screen_texture(RENDER_TARGET_SSAO_RAW, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	create_screen_texture(RENDER_TARGET_SSAO, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	/////////////
	// Composite
	/////////////

	create_screen_texture(RENDER_TARGET_COMPOSITE, swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT,
						  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
}

void Renderer::endFrame(unsigned int image_index)
{
	// Update debug info
	debug_info = RendererDebugInfo{};
	debug_info.descriptors_count = descriptors.size();
	debug_info.descriptor_bindings_count = descriptor_bindings.size();

	debug_info.descriptors_max_offset = 0;
	for (auto offset_pair : descriptors_offset)
	{
		for (auto offset : offset_pair.second)
		{
			if (offset > debug_info.descriptors_max_offset)
			{
				debug_info.descriptors_max_offset = offset;
			}
		}
	}

	// Reset offsets for uniform buffers
	for (auto &offset : descriptors_offset)
	{
		offset.second[image_index] = 0;
	}
}

void Renderer::setShadersUniformBuffer(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader, unsigned int binding, void *params_struct, size_t params_size, unsigned int image_index)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(VkWrapper::getMergedDescriptors(vertex_shader, fragment_shader));

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, vertex_shader->getHash());
	hash_combine(descriptor_hash, fragment_shader->getHash());

	size_t offset = descriptors_offset[descriptor_hash][image_index];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	size_t binding_hash = descriptor_hash;
	hash_combine(binding_hash, binding);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_binding_frame = descriptor_bindings[binding_hash][i];
		// If there is no descriptor binding for this offset, then create buffer for it and map
		if (current_binding_frame.bindings_per_offset.size() <= offset)
		{
			current_binding_frame.bindings_per_offset.resize(offset + 1);
			auto &current_binding = current_binding_frame.bindings_per_offset[offset];

			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

			current_binding.first = std::make_shared<Buffer>(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, current_binding.first->bufferHandle, params_size);
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[binding_hash][image_index].bindings_per_offset[offset];
	memcpy(current_binding.second, params_struct, params_size);
}

void Renderer::setShadersTexture(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader, unsigned int binding, std::shared_ptr<Texture> texture, unsigned int image_index, int mip, int face)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(VkWrapper::getMergedDescriptors(vertex_shader, fragment_shader));

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, vertex_shader->getHash());
	hash_combine(descriptor_hash, fragment_shader->getHash());
	size_t offset = descriptors_offset[descriptor_hash][image_index];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	// Update set
	DescriptorWriter writer;
	writer.writeImage(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texture->getImageView(mip, face), texture->sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.updateSet(descriptors[descriptor_hash][image_index].descriptor_per_offset[offset]);
}

void Renderer::bindShadersDescriptorSets(std::shared_ptr<Shader> vertex_shader, std::shared_ptr<Shader> fragment_shader, CommandBuffer &command_buffer, VkPipelineLayout pipeline_layout, unsigned int image_index)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(VkWrapper::getMergedDescriptors(vertex_shader, fragment_shader));

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, vertex_shader->getHash());
	hash_combine(descriptor_hash, fragment_shader->getHash());
	size_t offset = descriptors_offset[descriptor_hash][image_index];

	if (descriptors.find(descriptor_hash) != descriptors.end())
	{
		vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptors[descriptor_hash][image_index].descriptor_per_offset[offset], 0, nullptr);
	}

	// After every bind increment offset (because bind was made for a draw that uses this uniforms and we cant write to it)
	descriptors_offset[descriptor_hash][image_index] += 1;
}

void Renderer::ensureDescriptorsAllocated(DescriptorLayout descriptor_layout, size_t descriptor_hash, size_t offset)
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_descriptor = descriptors[descriptor_hash][i];
		// If there is no allocated descriptors for this offset, then allocate new descriptor set
		if (current_descriptor.descriptor_per_offset.size() <= offset)
		{
			current_descriptor.descriptor_per_offset.resize(offset + 1);
			current_descriptor.descriptor_per_offset[offset] = VkWrapper::global_descriptor_allocator->allocate(descriptor_layout.layout);
		}
	}
}
