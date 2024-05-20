#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
	vertex_shader = Shader::create("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/deffered_composite.frag", Shader::FRAGMENT_SHADER);
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
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

	// Uniforms
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DefferedCompositeRenderer::renderImgui()
{
}
