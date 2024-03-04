#include "pch.h"
#include "MeshRenderer.h"
#include <stb_image.h>

static struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

static VkDescriptorSetLayoutBinding descriptorSetLayoutBinding(uint32_t binding, VkDescriptorType descriptor_type, VkShaderStageFlags stage_flags, uint32_t count = 1)
{
	VkDescriptorSetLayoutBinding layout_binding{};
	layout_binding.binding = binding;
	layout_binding.descriptorCount = count;
	layout_binding.descriptorType = descriptor_type;
	layout_binding.pImmutableSamplers = nullptr;
	layout_binding.stageFlags = stage_flags;
	return layout_binding;
}

static VkDescriptorPool createDescriptorPool(uint32_t uniform_buffers_count, uint32_t samplers_count)
{
	VkDescriptorPool descriptor_pool;
	std::vector<VkDescriptorPoolSize> poolSizes{};

	if (uniform_buffers_count != 0)
		poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffers_count});

	if (samplers_count != 0)
		poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplers_count});

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	CHECK_ERROR(vkCreateDescriptorPool(VkWrapper::device->logicalHandle, &poolInfo, nullptr, &descriptor_pool));
	return descriptor_pool;
}

static VkWriteDescriptorSet bufferWriteDescriptorSet(VkDescriptorSet set, uint32_t binding, VkDescriptorType descriptor_type, VkDescriptorBufferInfo *descriptor_buffer_info, uint32_t descriptor_count = 1)
{
	VkWriteDescriptorSet write_descriptor_set{};
	write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor_set.dstSet = set;
	write_descriptor_set.dstBinding = binding;
	write_descriptor_set.dstArrayElement = 0;
	write_descriptor_set.descriptorType = descriptor_type;
	write_descriptor_set.descriptorCount = descriptor_count;
	write_descriptor_set.pBufferInfo = descriptor_buffer_info;
	write_descriptor_set.pImageInfo = nullptr;
	write_descriptor_set.pTexelBufferView = nullptr;
	return write_descriptor_set;
}

static VkWriteDescriptorSet imageWriteDescriptorSet(VkDescriptorSet set, uint32_t binding, VkDescriptorImageInfo *descriptor_image_info, uint32_t descriptor_count = 1)
{
	VkWriteDescriptorSet write_descriptor_set{};
	write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor_set.dstSet = set;
	write_descriptor_set.dstBinding = binding;
	write_descriptor_set.dstArrayElement = 0;
	write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write_descriptor_set.descriptorCount = descriptor_count;
	write_descriptor_set.pBufferInfo = nullptr;
	write_descriptor_set.pImageInfo = descriptor_image_info;
	write_descriptor_set.pTexelBufferView = nullptr;
	return write_descriptor_set;
}


MeshRenderer::MeshRenderer() : RendererBase()
{
	// Create descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT),
	};
	VkDescriptorSetLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	info.bindingCount = bindings.size();
	info.pBindings = bindings.data();

	CHECK_ERROR(vkCreateDescriptorSetLayout(VkWrapper::device->logicalHandle, &info, nullptr, &descriptor_set_layout));


	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;
	description.descriptor_set_layout = &descriptor_set_layout;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);


	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	image_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	image_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		image_uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		image_uniform_buffers[i]->map(&image_uniform_buffers_mapped[i]);
	}

	// Load image
	TextureDescription tex_description;
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	texture = std::make_shared<Texture>(tex_description);
	texture->load("assets/albedo2.png");

	// Create descriptor pool
	descriptor_pool = createDescriptorPool(MAX_FRAMES_IN_FLIGHT, MAX_FRAMES_IN_FLIGHT);

	// Create descriptor set
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptor_pool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();

	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	CHECK_ERROR(vkAllocateDescriptorSets(VkWrapper::device->logicalHandle, &allocInfo, image_descriptor_sets.data()));

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo buffer_info{};
		buffer_info.buffer = image_uniform_buffers[i]->bufferHandle;
		buffer_info.offset = 0;
		buffer_info.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo image_info{};
		image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		image_info.imageView = texture->imageView;
		image_info.sampler = texture->sampler;

		std::vector<VkWriteDescriptorSet> descriptorWrites = {
			bufferWriteDescriptorSet(image_descriptor_sets[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &buffer_info),
			imageWriteDescriptorSet(image_descriptor_sets[i], 1, &image_info),
		};

		vkUpdateDescriptorSets(VkWrapper::device->logicalHandle, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
	}

	// Load mesh
	mesh = std::make_shared<Engine::Mesh>("assets/model2.obj");
}

MeshRenderer::~MeshRenderer()
{
	vkDestroyDescriptorPool(VkWrapper::device->logicalHandle, descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void MeshRenderer::fillCommandBuffer(CommandBuffer & command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &image_descriptor_sets[image_index], 0, nullptr);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
}

void MeshRenderer::updateUniformBuffer(uint32_t image_index)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo{};
	ubo.model = /*glm::rotate(glm::mat4(1), time * glm::radians(45.0f), glm::vec3(0, 1, 0)) **/ glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(1, 0, 0)) * glm::translate(glm::mat4(1), glm::vec3(0, 0, 0));
	ubo.view = glm::lookAt(glm::vec3(2, -2, 4), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	ubo.proj = glm::perspective(glm::radians(45.0f), VkWrapper::swapchain->swapExtent.width / (float)VkWrapper::swapchain->swapExtent.height, 0.1f, 60.0f);
	//ubo.proj[1][1] *= -1;
	//ubo.proj = glm::mat4(1);
	//ubo.view = glm::mat4(1);
	memcpy(image_uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));
}
