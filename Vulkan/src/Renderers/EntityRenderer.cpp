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

void EntityRenderer::renderEntity(CommandBuffer &command_buffer, Entity entity)
{
	render_entity(command_buffer, entity);
}

void EntityRenderer::renderEntityShadow(CommandBuffer &command_buffer, glm::mat4 &transform_matrix, MeshRendererComponent &mesh_renderer)
{
	auto &p = VkWrapper::global_pipeline;
	ShadowPushConstact push_constant;
	push_constant.model = transform_matrix;
	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstact), &push_constant);

	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		const std::shared_ptr<Engine::Mesh> mesh = mesh_renderer.meshes[i].getMesh();

		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
	}
}

void EntityRenderer::render_entity(CommandBuffer &command_buffer, Entity entity)
{
	auto &p = VkWrapper::global_pipeline;
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
}
