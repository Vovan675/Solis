#include "pch.h"
#include "MeshRenderer.h"
#include "BindlessResources.h"
#include <stb_image.h>

static struct UniformBufferObject
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

MeshRenderer::MeshRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Engine::Mesh> mesh) : RendererBase()
{
	camera = cam;
	rotation = glm::rotate(glm::quat(), glm::vec3(glm::radians(90.0f), 0, 0));
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->mesh = mesh;
	this->texture = texture;

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

	// Create descriptor set layout
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptor_set_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptor_set_layout);
	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		image_descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(layouts[i]);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, image_uniform_buffers[i]->bufferHandle, sizeof(UniformBufferObject));
		writer.updateSet(image_descriptor_sets[i]);
	}

	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/simple.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;

	description.descriptor_set_layout = descriptor_set_layout;

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);
}

MeshRenderer::~MeshRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void MeshRenderer::fillCommandBuffer(CommandBuffer & command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
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
	UniformBufferObject ubo{};
	ubo.model = glm::scale(glm::mat4(1.0f), scale) * glm::mat4_cast(rotation) * glm::translate(glm::mat4(1.0f), position);
	ubo.view = camera->getView();
	ubo.proj = camera->getProj();
	memcpy(image_uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));
}
