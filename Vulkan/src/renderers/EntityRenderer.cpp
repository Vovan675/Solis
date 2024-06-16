#include "pch.h"
#include "EntityRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>

EntityRenderer::EntityRenderer(std::shared_ptr<Camera> cam, std::shared_ptr<Entity> entity): RendererBase()
{
	camera = cam;
	rotation = glm::quat();
	scale = glm::vec3(1.0f, 1.0f, 1.0f);
	this->entity = entity;
	this->texture = texture;

	vertex_shader = Shader::create("shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

EntityRenderer::~EntityRenderer()
{
}

void EntityRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render entity
	renderEntity(command_buffer, entity.get(), image_index);

	p->unbind(command_buffer);
}
 
void EntityRenderer::renderEntity(CommandBuffer &command_buffer, Entity *entity, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	// Render all meshes of this entity
	// TODO: Set materials
	for (int i = 0; i < entity->meshes.size(); i++)
	{
		const auto& mesh = entity->meshes[i];
		const Material& mat = entity->materials[i];
		// Push constant
		vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &mat.getPushConstant(entity->transform.model_matrix));
		
		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
	}
	
	for (auto child : entity->children)
	{
		renderEntity(command_buffer, child.get(), image_index);
	}
}
