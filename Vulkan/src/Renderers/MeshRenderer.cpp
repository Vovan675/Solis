#include "pch.h"
#include "MeshRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>

MeshRenderer::MeshRenderer(Ref<Engine::Mesh> mesh) : RendererBase()
{
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->mesh = mesh;
	this->texture = texture;

	vertex_shader = gDynamicRHI->createShader(L"shaders/opaque.vert", VERTEX_SHADER);
	fragment_shader = gDynamicRHI->createShader(L"shaders/opaque.frag", FRAGMENT_SHADER);
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::fillCommandBuffer(RHICommandList *cmd_list)
{
	/*
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	// TODO:
	//p->setVertexShader(vertex_shader);
	//p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Push constant
	glm::mat4 model = glm::mat4_cast(rotation) * glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), scale) * mesh->root_transform;
	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &mat.getPushConstant(model));

	// Render mesh

	// TODO: fix
	/*
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->buffer_handle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->buffer_handle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
	*/
}
