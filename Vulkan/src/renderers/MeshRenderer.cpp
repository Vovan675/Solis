#include "pch.h"
#include "MeshRenderer.h"
#include "BindlessResources.h"
#include <stb_image.h>

MeshRenderer::MeshRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Engine::Mesh> mesh) : RendererBase()
{
	camera = cam;
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->mesh = mesh;
	this->texture = texture;

	// Create uniform buffers
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		BufferDescription desc;
		desc.size = bufferSize;
		desc.useStagingBuffer = false;
		desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		uniform_buffers[i] = std::make_shared<Buffer>(desc);

		// Map gpu memory on cpu memory
		uniform_buffers[i]->map(&uniform_buffers_mapped[i]);
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
	descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		descriptor_sets[i] = VkWrapper::global_descriptor_allocator->allocate(descriptor_layout.layout);
	}

	// Update descriptor set
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		DescriptorWriter writer;
		writer.writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffers[i]->bufferHandle, sizeof(UniformBufferObject));
		writer.writeBuffer(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_uniform_buffers[i]->bufferHandle, sizeof(Material));
		writer.updateSet(descriptor_sets[i]);
	}

	reloadShaders();
}

MeshRenderer::~MeshRenderer()
{
	vkDestroyDescriptorSetLayout(VkWrapper::device->logicalHandle, descriptor_layout.layout, nullptr);
}

void MeshRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>(VkWrapper::device->logicalHandle, "shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

void MeshRenderer::fillCommandBuffer(CommandBuffer & command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseBlending(false);

	p->setPushConstantRanges({{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant)}});
	p->setDescriptorLayout(descriptor_layout);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 0, 1, &descriptor_sets[image_index], 0, nullptr);

	// Push constant
	PushConstant push_constant;
	push_constant.model = glm::mat4_cast(rotation) * glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), scale) * mesh->root_transform;
	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &push_constant);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
}

void MeshRenderer::updateUniformBuffer(uint32_t image_index)
{
	UniformBufferObject ubo{};
	ubo.view = camera->getView();
	ubo.proj = camera->getProj();
	ubo.camera_position = camera->getPosition();
	memcpy(uniform_buffers_mapped[image_index], &ubo, sizeof(ubo));

	memcpy(material_uniform_buffers_mapped[image_index], &mat, sizeof(mat));
}
