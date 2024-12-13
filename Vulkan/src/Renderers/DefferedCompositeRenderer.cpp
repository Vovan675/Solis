#include "pch.h"
#include "DefferedCompositeRenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DefferedCompositeRenderer::DefferedCompositeRenderer()
{
}

DefferedCompositeRenderer::~DefferedCompositeRenderer()
{
}

void DefferedCompositeRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	ubo.lighting_diffuse_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_DIFFUSE);
	ubo.lighting_specular_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_SPECULAR);
	ubo.albedo_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_ALBEDO);
	ubo.normal_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_NORMAL);
	ubo.depth_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_DEPTH_STENCIL);
	ubo.shading_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_SHADING);
	ubo.brdf_lut_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_BRDF_LUT);
	ubo.ssao_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO);

	auto &p = VkWrapper::global_pipeline;
	p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/deffered_composite.frag", Shader::FRAGMENT_SHADER));

	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, Renderer::getRenderTarget(RENDER_TARGET_IBL_IRRADIANCE));
	Renderer::setShadersTexture(p->getCurrentShaders(), 2, Renderer::getRenderTarget(RENDER_TARGET_IBL_PREFILER));

	// Uniforms
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DefferedCompositeRenderer::renderImgui()
{
}
