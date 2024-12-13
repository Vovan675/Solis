#include "pch.h"
#include "LutRenderer.h"
#include "BindlessResources.h"

void LutRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/ibl/brdf_lut.frag", Shader::FRAGMENT_SHADER));

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}
