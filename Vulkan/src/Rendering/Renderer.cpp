#include "pch.h"
#include "Renderer.h"
#include "BindlessResources.h"

RendererDebugInfo Renderer::debug_info = {};
std::array<std::shared_ptr<Texture>, RENDER_TARGET_TEXTURES_COUNT> Renderer::screen_resources;
std::unordered_map<size_t, std::array<Renderer::PerFrameDescriptor, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptors;
std::unordered_map<size_t, std::array<size_t, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptors_offset;
std::unordered_map<size_t, std::array<Renderer::PerFrameDescriptorBinding, MAX_FRAMES_IN_FLIGHT>> Renderer::descriptor_bindings;

Renderer::DefaultUniforms Renderer::default_uniforms;
DescriptorLayout Renderer::default_descriptor_layout;
std::shared_ptr<Camera> Renderer::camera;

std::array<std::vector<std::pair<RESOURCE_TYPE, void *>>, MAX_FRAMES_IN_FLIGHT> Renderer::deletion_queue;
int Renderer::current_frame_in_flight = 0;
int Renderer::current_image_index = 0;
uint64_t Renderer::current_frame = 0;
uint32_t Renderer::timestamp_index = 0;

glm::ivec2 Renderer::viewport_size;

void Renderer::init()
{
	viewport_size = VkWrapper::swapchain->getSize();
	recreateScreenResources();

	DescriptorLayoutBuilder layout_builder;
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // 0 - Default uniforms
	default_descriptor_layout = layout_builder.build(VK_SHADER_STAGE_ALL);
}

void Renderer::shutdown()
{
	for (size_t i = 0; i < RENDER_TARGET_TEXTURES_COUNT; i++)
		screen_resources[i] = nullptr;

	descriptor_bindings.clear();

	deleteResources(current_frame_in_flight);
	vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
	Renderer::endFrame(0);
}

void Renderer::recreateScreenResources()
{
	TextureDescription description;
	description.width = viewport_size.x;
	description.height = viewport_size.y;

	auto create_screen_texture = [&description](RENDER_TARGETS rt, VkFormat format, uint32_t usage_flags, const char *name = nullptr, bool anisotropy = false, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
	{
		description.image_format = format;
		description.usage_flags = usage_flags;
		description.sampler_address_mode = sampler_address_mode;
		description.anisotropy = anisotropy;
		screen_resources[rt] = Texture::create(description);
		screen_resources[rt]->fill();
		if (name)
			screen_resources[rt]->setDebugName(name);

		BindlessResources::addTexture(screen_resources[rt].get());
	};


	auto swapchain_format = VkWrapper::swapchain->surface_format.format;
	create_screen_texture(RENDER_TARGET_FINAL_NO_POST, swapchain_format, TEXTURE_USAGE_ATTACHMENT, "No Post Final Image");
	create_screen_texture(RENDER_TARGET_FINAL, swapchain_format, TEXTURE_USAGE_ATTACHMENT, "Final Image");
}

void Renderer::setViewportSize(glm::ivec2 size)
{
	if (viewport_size == size)
		return;
	viewport_size = size;
	recreateScreenResources();
}

void Renderer::beginFrame(unsigned int current_frame_in_flight, unsigned int current_image_index)
{
	current_frame++;
	Renderer::current_frame_in_flight = current_frame_in_flight;
	Renderer::current_image_index = current_image_index;

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

	timestamp_index = 0;

	deleteResources(current_frame_in_flight);
}

void Renderer::endFrame(unsigned int image_index)
{
	// Reset offsets for uniform buffers
	for (auto &offset : descriptors_offset)
	{
		offset.second[image_index] = 0;
	}
	
	int count = timestamp_index;
	if (count > 0)
		vkGetQueryPoolResults(VkWrapper::device->logicalHandle, VkWrapper::device->query_pools[current_frame_in_flight], 0, count, VkWrapper::device->time_stamps[current_frame_in_flight].size() * sizeof(uint64_t), VkWrapper::device->time_stamps[current_frame_in_flight].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
}

void Renderer::deleteResources(unsigned int frame_in_flight)
{
	if (!deletion_queue[frame_in_flight].empty())
	{
		vkDeviceWaitIdle(VkWrapper::device->logicalHandle);

		for (auto resource : deletion_queue[frame_in_flight])
		{
			switch (resource.first)
			{
				case RESOURCE_TYPE_SAMPLER: 
					vkDestroySampler(VkWrapper::device->logicalHandle, (VkSampler)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE_VIEW:
					vkDestroyImageView(VkWrapper::device->logicalHandle, (VkImageView)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE:
					vkDestroyImage(VkWrapper::device->logicalHandle, (VkImage)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_MEMORY:
					vmaFreeMemory(VkWrapper::allocator, (VmaAllocation)resource.second);
					break;
				case RESOURCE_TYPE_BUFFER:
					vkDestroyBuffer(VkWrapper::device->logicalHandle, (VkBuffer)resource.second, nullptr);
					break;
			}
		}

		deletion_queue[frame_in_flight].clear();
	}
}

uint32_t Renderer::beginTimestamp()
{
	uint32_t current_timestep = timestamp_index;
	vkCmdWriteTimestamp2(getCurrentCommandBuffer().get_buffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkWrapper::device->query_pools[current_frame_in_flight], timestamp_index++);
	timestamp_index++;
	return current_timestep;
}

void Renderer::endTimestamp(uint32_t index)
{
	vkCmdWriteTimestamp2(getCurrentCommandBuffer().get_buffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VkWrapper::device->query_pools[current_frame_in_flight], index);
}

float Renderer::getTimestampTime(uint32_t index)
{
	float period = VkWrapper::device->physicalProperties.properties.limits.timestampPeriod;
	float delta_in_ms = (VkWrapper::device->time_stamps[current_frame_in_flight][index + 1] - VkWrapper::device->time_stamps[current_frame_in_flight][index]) * period / 1000000.0f;
	return delta_in_ms;
}

void Renderer::setShadersAccelerationStructure(std::vector<std::shared_ptr<Shader>> shaders, VkAccelerationStructureKHR *acceleration_structure, unsigned int binding)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	// Update set
	DescriptorWriter writer;
	writer.writeAccelerationStructure(binding, acceleration_structure);
	writer.updateSet(descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset]);
}

void Renderer::setShadersStorageBuffer(std::vector<std::shared_ptr<Shader>> shaders, unsigned int binding, void *params_struct, size_t params_size)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

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
			desc.bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			current_binding.first = Buffer::create(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, current_binding.first->bufferHandle, params_size);
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[binding_hash][current_frame_in_flight].bindings_per_offset[offset];
	memcpy(current_binding.second, params_struct, params_size);
}

void Renderer::setShadersStorageBuffer(std::vector<std::shared_ptr<Shader>> shaders, unsigned int binding, std::shared_ptr<Buffer> buffer)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

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
			current_binding.first = buffer;

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, current_binding.first->bufferHandle, buffer->getSize());
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}
}

void Renderer::setShadersUniformBuffer(std::vector<std::shared_ptr<Shader>> shaders, unsigned int binding, void *params_struct, size_t params_size)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

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

			current_binding.first = Buffer::create(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, current_binding.first->bufferHandle, params_size);
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[binding_hash][current_frame_in_flight].bindings_per_offset[offset];
	memcpy(current_binding.second, params_struct, params_size);
}

void Renderer::setShadersTexture(std::vector<std::shared_ptr<Shader>> shaders, unsigned int binding, std::shared_ptr<Texture> texture, int mip, int face, bool is_uav)
{
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	VkImageLayout image_layout = texture->isDepthTexture() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (is_uav && (texture->getUsageFlags() & TEXTURE_USAGE_STORAGE))
	{
		descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		image_layout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// Update set
	DescriptorWriter writer;
	writer.writeImage(binding, descriptor_type, texture->getImageView(mip, face), texture->getSampler(), image_layout);
	writer.updateSet(descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset]);
}

void Renderer::bindShadersDescriptorSets(std::vector<std::shared_ptr<Shader>> shaders, CommandBuffer &command_buffer, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point)
{
	// Bind bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Bind default uniforms
	size_t default_uniforms_offset = descriptors_offset[0][current_frame_in_flight];
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 2, 1, &descriptors[0][current_frame_in_flight].descriptor_per_offset[default_uniforms_offset], 0, nullptr);

	// Bind custom descriptor
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	if (descriptors.find(descriptor_hash) != descriptors.end())
	{
		vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 0, 1, &descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset], 0, nullptr);
	}

	// After every bind increment offset (because bind was made for a draw that uses this uniforms and we cant write to it)
	descriptors_offset[descriptor_hash][current_frame_in_flight] += 1;
}

void Renderer::updateDefaultUniforms(float delta_time)
{
	default_uniforms.view = camera->getView();
	default_uniforms.iview = glm::inverse(camera->getView());
	default_uniforms.projection = camera->getProj();
	default_uniforms.iprojection = glm::inverse(camera->getProj());
	default_uniforms.camera_position = glm::vec4(camera->getPosition(), 1.0);
	default_uniforms.swapchain_size = glm::vec4(Renderer::getViewportSize(), 1.0f / glm::vec2(Renderer::getViewportSize()));
	default_uniforms.time += delta_time;

	// After every updating (changing) increment offset because we can't override previous descriptor layout that is used in this frame
	descriptors_offset[0][current_frame_in_flight] += 1;

	size_t offset = descriptors_offset[0][current_frame_in_flight];

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_descriptor = descriptors[0][i];
		// If there is no allocated descriptors for this offset, then allocate new descriptor set
		if (current_descriptor.descriptor_per_offset.size() <= offset)
		{
			current_descriptor.descriptor_per_offset.resize(offset + 1);
			current_descriptor.descriptor_per_offset[offset] = VkWrapper::global_descriptor_allocator->allocate(default_descriptor_layout.layout);
		}

		auto &current_binding_frame = descriptor_bindings[0][i];
		// If there is no descriptor binding for this offset, then create buffer for it and map
		if (current_binding_frame.bindings_per_offset.size() <= offset)
		{
			current_binding_frame.bindings_per_offset.resize(offset + 1);
			auto &current_binding = current_binding_frame.bindings_per_offset[offset];

			BufferDescription desc;
			desc.size = sizeof(DefaultUniforms);
			desc.useStagingBuffer = false;
			desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

			current_binding.first = Buffer::create(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, current_binding.first->bufferHandle, sizeof(DefaultUniforms));
			writer.updateSet(descriptors[0][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[0][current_frame_in_flight].bindings_per_offset[offset];
	memcpy(current_binding.second, &default_uniforms, sizeof(DefaultUniforms));
}

const Renderer::DefaultUniforms Renderer::getDefaultUniforms()
{
	return default_uniforms;
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
