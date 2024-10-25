#include "pch.h"
#include "PrefilterRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"

PrefilterRenderer::PrefilterRenderer(): RendererBase()
{
	compute_shader = Shader::create("shaders/ibl/prefilter.comp", Shader::COMPUTE_SHADER);
}

void PrefilterRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	cube_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	output_prefilter_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_GENERAL);
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setIsComputePipeline(true);
	p->setComputeShader(compute_shader);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, output_prefilter_texture, -1, -1, true);
	Renderer::setShadersTexture(p->getCurrentShaders(), 2, cube_texture);
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);

	vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantFrag), &constants_frag);


	vkCmdDispatch(command_buffer.get_buffer(), output_prefilter_texture->getWidth() / 32, output_prefilter_texture->getHeight() / 32, 6);

	p->unbind(command_buffer);
}
