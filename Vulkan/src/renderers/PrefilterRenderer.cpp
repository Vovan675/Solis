#include "pch.h"
#include "PrefilterRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

PrefilterRenderer::PrefilterRenderer(): RendererBase()
{
	// Load image
	TextureDescription tex_description{};
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	tex_description.is_cube = true;

	mesh = std::make_shared<Engine::Mesh>("assets/cube.fbx");

	reloadShaders();
}

PrefilterRenderer::~PrefilterRenderer()
{
}

void PrefilterRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/ibl/cubemap_filter.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/ibl/prefilter.frag", Shader::FRAGMENT_SHADER);
}

void PrefilterRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setCullMode(VK_CULL_MODE_BACK_BIT);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	Renderer::setShadersTexture(vertex_shader, fragment_shader, 1, cube_texture, image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantVert), &constants_vert);
	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstantVert), sizeof(PushConstantFrag), &constants_frag);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
}
