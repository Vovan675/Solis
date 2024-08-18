#include "pch.h"
#include "EntityRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>
#include "Scene/Components.h"

EntityRenderer::EntityRenderer(): RendererBase()
{
	vertex_shader = Shader::create("shaders/opaque.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/opaque.frag", Shader::FRAGMENT_SHADER);
	default_material = std::make_shared<Material>();
}

EntityRenderer::~EntityRenderer()
{
}

void EntityRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{

}
 
void EntityRenderer::renderEntity(CommandBuffer &command_buffer, Entity entity, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	render_entity(command_buffer, entity, image_index);

	p->unbind(command_buffer);
}

void EntityRenderer::renderEntityShadow(CommandBuffer &command_buffer, Entity entity, uint32_t image_index, glm::mat4 light_space, glm::vec3 light_pos)
{
	ShadowUBO ubo;
	ubo.light_space_matrix = light_space;
	ubo.light_pos = light_pos;

	auto &p = VkWrapper::global_pipeline;

	auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
	auto &transform = entity.getComponent<TransformComponent>();

	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		const std::shared_ptr<Engine::Mesh> mesh = mesh_renderer.meshes[i].getMesh();

		ubo.model = entity.getWorldTransformMatrix();
		Renderer::setShadersUniformBuffer(VkWrapper::global_pipeline->getVertexShader(), VkWrapper::global_pipeline->getFragmentShader(), 0, &ubo, sizeof(ShadowUBO), image_index);
		Renderer::bindShadersDescriptorSets(VkWrapper::global_pipeline->getVertexShader(), VkWrapper::global_pipeline->getFragmentShader(), command_buffer, p->getPipelineLayout(), image_index);

		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
	}

	for (auto child_id : transform.children)
	{
		renderEntityShadow(command_buffer, Entity(child_id, entity.getScene()), image_index, light_space, light_pos);
	}
}

void EntityRenderer::render_entity(CommandBuffer &command_buffer, Entity entity, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
	auto &transform = entity.getComponent<TransformComponent>();

	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		const std::shared_ptr<Engine::Mesh> mesh = mesh_renderer.meshes[i].getMesh();
		const auto material = mesh_renderer.materials.size() > i ? mesh_renderer.materials[i] : default_material;
		// Push constant
		vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &material->getPushConstant(entity.getWorldTransformMatrix()));

		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
	}

	for (auto child_id : transform.children)
	{
		render_entity(command_buffer, Entity(child_id, entity.getScene()), image_index);
	}
}
