#include "pch.h"
#include "IrradianceRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"

IrradianceRenderer::IrradianceRenderer(): RendererBase()
{
	compute_shader = Shader::create("shaders/ibl/irradiance.comp", Shader::COMPUTE_SHADER);
}

void IrradianceRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	cube_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	output_irradiance_texture->transitLayout(command_buffer, TEXTURE_LAYOUT_GENERAL);
	// Very heavy computation
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setIsComputePipeline(true);
	p->setComputeShader(compute_shader);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, output_irradiance_texture, -1, -1, true);
	Renderer::setShadersTexture(p->getCurrentShaders(), 2, cube_texture);
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);

	vkCmdDispatch(command_buffer.get_buffer(), output_irradiance_texture->getWidth() / 32, output_irradiance_texture->getHeight() / 32, 6);

	p->unbind(command_buffer);
}
