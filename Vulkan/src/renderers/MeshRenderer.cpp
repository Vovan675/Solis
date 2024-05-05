#include "pch.h"
#include "MeshRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>

MeshRenderer::MeshRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Engine::Mesh> mesh) : RendererBase()
{
	camera = cam;
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->mesh = mesh;
	this->texture = texture;

	reloadShaders();
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

void MeshRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	UniformBufferObject ubo{};
	ubo.view = camera->getView();
	ubo.proj = camera->getProj();
	ubo.camera_position = camera->getPosition();
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(UniformBufferObject), image_index);
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 1, &mat, sizeof(Material), image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

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
