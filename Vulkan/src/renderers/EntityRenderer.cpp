#include "pch.h"
#include "EntityRenderer.h"
#include "BindlessResources.h"
#include <stb_image.h>

static struct UniformBufferObject
{
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::vec3 camera_position;
};

static struct PushConstant
{
	alignas(16) glm::mat4 model;
};

EntityRenderer::EntityRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Entity> entity): RendererBase()
{
	camera = cam;
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->entity = entity;
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

	bufferSize = sizeof(Material);

	material_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	material_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		material_uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		material_uniform_buffers[i]->map(&material_uniform_buffers_mapped[i]);
	}

	// Create descriptor set layout
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();
	layout_builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layout_builder.add_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	descriptor_set_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		image_descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_set_layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, image_uniform_buffers[i]->bufferHandle, sizeof(UniformBufferObject));
		writer.writeBuffer(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_uniform_buffers[i]->bufferHandle, sizeof(Material));
		writer.updateSet(image_descriptor_sets[i]);
	}

	recreatePipeline();
}

EntityRenderer::~EntityRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_set_layout, nullptr);
}

void EntityRenderer::recreatePipeline()
{
	// Create pipeline
	auto vertShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.vert", Shader::VERTEX_SHADER);
	auto fragShader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.frag", Shader::FRAGMENT_SHADER);

	PipelineDescription description{};
	description.vertex_shader = vertShader;
	description.fragment_shader = fragShader;

	description.descriptor_set_layout = descriptor_set_layout;
	description.color_formats = {VkWrapper::swapchain->surface_format.format, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM};
	description.use_blending = false;

	VkPushConstantRange push_constant_range{};
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(PushConstant);
	push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	description.push_constant_ranges = {push_constant_range};

	pipeline = std::make_shared<Pipeline>();
	pipeline->create(description);
}

void EntityRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	vkCmdBindPipeline(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline_layout, 0, 1, &image_descriptor_sets[image_index], 0, nullptr);

	// Render entity
	renderEntity(command_buffer, entity.get());
}

void EntityRenderer::renderEntity(CommandBuffer &command_buffer, Entity *entity)
{
	// Render all meshes of this entity
	// TODO: Set materials
	for (auto mesh : entity->meshes)
	{
		// Push constant
		PushConstant push_constant;
		push_constant.model = entity->transform.model_matrix;
		vkCmdPushConstants(command_buffer.get_buffer(), pipeline->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push_constant);
		
		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
	}
	
	for (auto child : entity->children)
	{
		renderEntity(command_buffer, child.get());
	}
}

void EntityRenderer::updateUniformBuffer(uint32_t image_index)
{
	UniformBufferObject ubo{};
	ubo.view = camera->getView();
	ubo.proj = camera->getProj();
	ubo.camera_position = camera->getPosition();
	memcpy(image_uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));

	memcpy(material_uniform_buffers_mapped[image_index], &mat, sizeof(mat));
}