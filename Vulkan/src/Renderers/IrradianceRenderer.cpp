#include "pch.h"
#include "IrradianceRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	// Load image
	TextureDescription tex_description{};
	tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	tex_description.is_cube = true;
	
	auto model = AssetManager::getAsset<Model>("assets/cube.fbx");
	mesh = model->getRootNode()->children[0]->meshes[0];

	vertex_shader = Shader::create("shaders/ibl/cubemap_filter.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/ibl/irradiance.frag", Shader::FRAGMENT_SHADER);
}

IrradianceRenderer::~IrradianceRenderer()
{
}

void IrradianceRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	// Very heavy computation
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setCullMode(VK_CULL_MODE_BACK_BIT);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersTexture(vertex_shader, fragment_shader, 1, cube_texture, image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &constants);

	// Render mesh
	VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

	p->unbind(command_buffer);
}
