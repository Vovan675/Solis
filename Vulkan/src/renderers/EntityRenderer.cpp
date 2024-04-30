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
	descriptor_layout = layout_builder.build(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	// Create descriptor set
	image_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		image_descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_layout.layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, image_uniform_buffers[i]->bufferHandle, sizeof(UniformBufferObject));
		writer.writeBuffer(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_uniform_buffers[i]->bufferHandle, sizeof(Material));
		writer.updateSet(image_descriptor_sets[i]);
	}

	reloadShaders();
}

EntityRenderer::~EntityRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_layout.layout, nullptr);
}

void EntityRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

void EntityRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseBlending(false);
	p->setPushConstantRanges({{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant)}});

	p->setDescriptorLayout(descriptor_layout);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 0, 1, &image_descriptor_sets[image_index], 0, nullptr);

	// Render entity
	renderEntity(command_buffer, entity.get());

	p->unbind(command_buffer);
}

void EntityRenderer::renderEntity(CommandBuffer &command_buffer, Entity *entity)
{
	auto &p = VkWrapper::global_pipeline;
	// Render all meshes of this entity
	// TODO: Set materials
	for (auto mesh : entity->meshes)
	{
		// Push constant
		PushConstant push_constant;
		push_constant.model = entity->transform.model_matrix;
		vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push_constant);
		
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