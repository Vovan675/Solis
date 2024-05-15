#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
	reloadShaders();
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
}

void DefferedCompositeRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/deffered_composite.frag", Shader::FRAGMENT_SHADER);
}

void DefferedCompositeRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);
	
	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);

	p->setUseVertices(false);

	p->flush();
	p->bind(command_buffer);

	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(UBO), image_index);
	Renderer::setShadersTexture(vertex_shader, fragment_shader, 1, irradiance_cubemap, image_index);
	Renderer::setShadersTexture(vertex_shader, fragment_shader, 2, prefilter_cubemap, image_index);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DefferedCompositeRenderer::renderImgui()
{
}
