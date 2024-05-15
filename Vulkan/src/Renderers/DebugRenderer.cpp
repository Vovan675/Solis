#include "pch.h"
#include "DebugRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DebugRenderer::DebugRenderer()
{
	reloadShaders();
}

DebugRenderer::~DebugRenderer()
{
}

void DebugRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/debug_quad.frag", Shader::FRAGMENT_SHADER);
}

void DebugRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseVertices(false);
	p->setUseBlending(false);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(PresentUBO), image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}
