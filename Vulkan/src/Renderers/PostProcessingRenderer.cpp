#include "pch.h"
#include "imgui.h"
#include "PostProcessingRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

PostProcessingRenderer::PostProcessingRenderer()
{
	reloadShaders();
}

PostProcessingRenderer::~PostProcessingRenderer()
{
}

void PostProcessingRenderer::reloadShaders()
{
	vertex_shader = std::make_shared<Shader>("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = std::make_shared<Shader>("shaders/post_processing.frag", Shader::FRAGMENT_SHADER);
}

void PostProcessingRenderer::fillCommandBuffer(CommandBuffer &command_buffer, uint32_t image_index)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets, nullptr);
	p->setUseVertices(false);

	p->flush();
	p->bind(command_buffer);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	Renderer::setShadersUniformBuffer(vertex_shader, fragment_shader, 0, &ubo, sizeof(UBO), image_index);
	Renderer::bindShadersDescriptorSets(vertex_shader, fragment_shader, command_buffer, p->getPipelineLayout(), image_index);

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void PostProcessingRenderer::renderImgui()
{
	bool use_vignette_bool = ubo.use_vignette > 0.5f;
	if (ImGui::Checkbox("Vignette", &use_vignette_bool))
	{
		ubo.use_vignette = use_vignette_bool ? 1.0f : 0.0f;
	}

	if (ubo.use_vignette)
	{
		ImGui::SliderFloat("Vignette Radius", &ubo.vignette_radius, 0.1f, 1.0f);
		ImGui::SliderFloat("Vignette Smoothness", &ubo.vignette_smoothness, 0.1f, 1.0f);
	}
}
