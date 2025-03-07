#include "pch.h"
#include "EntityRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include <stb_image.h>
#include "Scene/Components.h"

EntityRenderer::EntityRenderer(): RendererBase()
{
	// TODO: fix
	return;
	vertex_shader = gDynamicRHI->createShader(L"shaders/opaque.vert", VERTEX_SHADER);
	fragment_shader = gDynamicRHI->createShader(L"shaders/opaque.frag", FRAGMENT_SHADER);
	default_material = new Material();
}

EntityRenderer::~EntityRenderer()
{
}

void EntityRenderer::renderEntity(RHICommandList *cmd_list, Entity entity)
{
	render_entity(cmd_list, entity);
}

void EntityRenderer::renderEntityShadow(RHICommandList *cmd_list, glm::mat4 &transform_matrix, MeshRendererComponent &mesh_renderer)
{
	auto &p = gGlobalPipeline;
	ShadowPushConstact push_constant;
	push_constant.model = transform_matrix;
	////vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstact), &push_constant);

	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		const auto &mesh = mesh_renderer.meshes[i].getMesh();

		// Render mesh
		// TODO: fix
		/*
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->buffer_handle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->buffer_handle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
		*/
	}
}

void EntityRenderer::render_entity(RHICommandList *cmd_list, Entity entity)
{
	auto &p = gGlobalPipeline;
	auto &mesh_renderer = entity.getComponent<MeshRendererComponent>();
	auto &transform = entity.getComponent<TransformComponent>();

	for (int i = 0; i < mesh_renderer.meshes.size(); i++)
	{
		const auto &mesh = mesh_renderer.meshes[i].getMesh();
		const auto material = mesh_renderer.materials.size() > i ? mesh_renderer.materials[i] : default_material;
		// Push constant
		////vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Material::PushConstant), &material->getPushConstant(entity.getWorldTransformMatrix()));

		// Render mesh
		// TODO: fix
		/*
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->buffer_handle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->buffer_handle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		Renderer::addDrawCalls(1);
		*/
	}
}
