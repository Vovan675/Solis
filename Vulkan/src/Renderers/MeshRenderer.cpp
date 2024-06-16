#include "pch.h"
#include "MeshRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>

MeshRenderer::MeshRenderer(std::shared_ptr<Engine::Mesh> mesh) : RendererBase()
{
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->mesh = mesh;
	this->texture = texture;

	vertex_shader = Shader::create("shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Push constant
	glm::mat4 model = glm::mat4_cast(rotation) * glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), scale) * mesh->root_transform;
	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &mat.getPushConstant(model));

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
}
