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

void DefferedCompositeRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);
	p->setUseBlending(false);
	
	p->setRenderTargets(VkWrapper::current_render_targets);

	p->setUseVertices(false);

	p->flush();
	p->bind(command_buffer);

	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, irradiance_cubemap);
	Renderer::setShadersTexture(p->getCurrentShaders(), 2, prefilter_cubemap);

	// Uniforms
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DefferedCompositeRenderer::renderImgui()
{
}
