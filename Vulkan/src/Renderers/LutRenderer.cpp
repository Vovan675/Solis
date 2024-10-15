#include "pch.h"
#include "LutRenderer.h"
#include "BindlessResources.h"

LutRenderer::LutRenderer()
{
	DescriptorLayoutBuilder layout_builder;
	layout_builder.clear();

	vertex_shader = Shader::create("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/ibl/brdf_lut.frag", Shader::FRAGMENT_SHADER);
}

LutRenderer::~LutRenderer()
{
}

void LutRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseVertices(false);
	p->setUseBlending(false);
	p->setCullMode(VK_CULL_MODE_BACK_BIT);

	p->flush();
	p->bind(command_buffer);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}
